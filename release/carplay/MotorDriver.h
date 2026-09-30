#pragma once

#include <Arduino.h>
#include <esp_arduino_version.h>
#include "CarConfig.h"

// Maker ESP32: M1 左前，M2 右前，M3 左后，M4 右后。
// 引脚顺序沿用 motorTest；官方资料中的 M4 顺序相反。
// 架空车轮后，用 CarConfig::MOTOR_POLARITY 校准每轮方向：正值应朝车头。
class MotorDriver {
 public:
  bool begin() {
    if (ready_) return true;

    for (uint8_t i = 0; i < kPinCount; ++i) {
      holdLow(pins_[i]);
    }
    for (uint8_t motor = 0; motor < kMotorCount; ++motor) {
      lastSpeed_[motor] = 0;
      if (CarConfig::MOTOR_POLARITY[motor] != 1 &&
          CarConfig::MOTOR_POLARITY[motor] != -1) {
        releasePwm();
        return false;
      }
    }

#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    for (uint8_t i = 0; i < kPinCount; ++i) {
      // ledcAttach 创建占空比为 0 的通道；初始化结束前不施加动力。
      if (!ledcAttach(pins_[i], kPwmFrequency, kPwmBits)) {
        releasePwm();
        return false;
      }
      attached_[i] = true;
      if (!ledcWrite(pins_[i], 0)) {
        releasePwm();
        return false;
      }
    }
#else
    // 2.x 按通道写 PWM；先配置并清零全部通道，再连接到 GPIO。
    for (uint8_t channel = 0; channel < kPinCount; ++channel) {
      if (ledcSetup(channel, kPwmFrequency, kPwmBits) <= 0) {
        releasePwm();
        return false;
      }
      ledcWrite(channel, 0);
    }
    for (uint8_t i = 0; i < kPinCount; ++i) {
      // Arduino-ESP32 2.x 的 ledcAttachPin 无返回值。
      ledcAttachPin(pins_[i], i);
      attached_[i] = true;
    }
#endif

    ready_ = true;
    return true;
  }

  // 统一左右轮接口，范围 -255..255；正值表示校准后的前进方向。
  void setWheels(int left, int right) {
    if (!ready_) return;
    left = constrain(left, -255, 255);
    right = constrain(right, -255, 255);
    setMotor(0, left);   // M1 左前
    setMotor(1, right);  // M2 右前
    setMotor(2, left);   // M3 左后
    setMotor(3, right);  // M4 右后
  }

  void setAuxLevel(int level) {
    if (!ready_) return;
    if (level < 0 || level > 3) level = 0;
    if (level == lastAuxLevel_) return;
    writeDuty(8, CarConfig::AUX_MOTOR_PWM[level]);
    lastAuxLevel_ = level;
  }

  void stop() { setWheels(0, 0); setAuxLevel(0); }

 private:
  static constexpr uint8_t kMotorCount = 4;
  static constexpr uint8_t kPinCount = 9;
  static constexpr uint32_t kPwmFrequency = 5000;
  static constexpr uint8_t kPwmBits = 8;

  const uint8_t pins_[kPinCount] = {27, 13, 4, 2, 17, 12, 14, 15, CarConfig::AUX_MOTOR_PIN};
  bool attached_[kPinCount] = {};
  int lastSpeed_[kMotorCount] = {};
  bool ready_ = false;
  int lastAuxLevel_ = 0;

  static void holdLow(uint8_t pin) {
    digitalWrite(pin, LOW);
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
  }

  void writeDuty(uint8_t pinIndex, uint8_t duty) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
    ledcWrite(pins_[pinIndex], duty);
#else
    ledcWrite(pinIndex, duty);
#endif
  }

  void setMotor(uint8_t motor, int speed) {
    speed *= CarConfig::MOTOR_POLARITY[motor];
    const int previous = lastSpeed_[motor];
    if (speed == previous) return;

    const uint8_t forward = motor * 2;
    const uint8_t reverse = forward + 1;
    if (speed == 0) {
      writeDuty(forward, 0);
      writeDuty(reverse, 0);
    } else {
      // 先关掉相反方向，再开启目标方向，避免换向瞬间双路同时驱动。
      writeDuty(speed > 0 ? reverse : forward, 0);
      if ((previous > 0 && speed < 0) || (previous < 0 && speed > 0)) {
        delayMicroseconds(100);
      }
      writeDuty(speed > 0 ? forward : reverse,
                static_cast<uint8_t>(speed > 0 ? speed : -speed));
    }
    lastSpeed_[motor] = speed;
  }

  void releasePwm() {
    ready_ = false;
    lastAuxLevel_ = 0;
    // 先停止所有已连接通道，随后逐个解除 PWM 并恢复推挽低电平。
    for (uint8_t i = 0; i < kPinCount; ++i) {
      if (attached_[i]) writeDuty(i, 0);
    }
    for (uint8_t i = 0; i < kPinCount; ++i) {
      if (attached_[i]) {
#if ESP_ARDUINO_VERSION >= ESP_ARDUINO_VERSION_VAL(3, 0, 0)
        ledcDetach(pins_[i]);
#else
        ledcDetachPin(pins_[i]);
#endif
        attached_[i] = false;
      }
      holdLow(pins_[i]);
    }
    for (uint8_t motor = 0; motor < kMotorCount; ++motor) {
      lastSpeed_[motor] = 0;
    }
  }
};
