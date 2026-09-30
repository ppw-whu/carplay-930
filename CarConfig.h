#pragma once
#include <stdint.h>

// Arduino IDE 打开 carplay.ino 后，在这个标签页填写配置。
namespace CarConfig {
// 小车自己建立 2.4 GHz 热点。电脑/手机直接连接，无需路由器。
constexpr char WIFI_SSID[] = "Carplay-ESP32";
constexpr char WIFI_PASSWORD[] = "carplay123";  // 热点密码，8~63 个字符

// 热点的固定地址；电脑连接热点后会自动获得同网段 IP。
constexpr uint8_t LOCAL_IP[4] = {192, 168, 4, 1};
constexpr uint8_t SUBNET[4] = {255, 255, 255, 0};

// 填手柄背面的配对码，例如 02:00:01:23:45:67（每个手柄不同）。
// 留空时不启动蓝牙，仍可通过 Wi-Fi 控制。
constexpr char PS3_HOST_MAC[] = "20:00:00:01:70:61";

// M1左前、M2右前、M3左后、M4右后。
// 正速度应让每个车轮向车头方向转；反转的轮子将对应 1 改成 -1。
constexpr int8_t MOTOR_POLARITY[4] = {1, 1, 1, 1};
constexpr int PS3_DEFAULT_SPEED_PERCENT = 80; // □ 每按一次增加20%，100%后回到20%
constexpr int JOYSTICK_DEADZONE = 15;
// GPIO32 舵机口的 S 脚 -> 高电平导通的 MOSFET 模块信号输入。
// 这是直流电机 PWM，不是舵机脉宽；档位 0/1/2/3 对应关闭/低/中/高。
constexpr uint8_t AUX_MOTOR_PIN = 32;
constexpr uint8_t AUX_MOTOR_PWM[4] = {0, 85, 170, 255};
static_assert(AUX_MOTOR_PWM[0] == 0 && AUX_MOTOR_PWM[1] > 0 &&
              AUX_MOTOR_PWM[1] < AUX_MOTOR_PWM[2] &&
              AUX_MOTOR_PWM[2] < AUX_MOTOR_PWM[3], "Aux PWM must increase from off");
constexpr uint32_t COMMAND_TIMEOUT_MS = 500;
constexpr uint32_t MAX_WIFI_DURATION_MS = 2000;
constexpr uint32_t MOTOR_PERIOD_MS = 10;
static_assert(PS3_DEFAULT_SPEED_PERCENT >= 20 && PS3_DEFAULT_SPEED_PERCENT <= 100 &&
              PS3_DEFAULT_SPEED_PERCENT % 20 == 0, "PS3 speed must be 20/40/60/80/100 percent");
static_assert(JOYSTICK_DEADZONE >= 0 && JOYSTICK_DEADZONE < 127, "Invalid joystick deadzone");
static_assert(COMMAND_TIMEOUT_MS >= 100 && COMMAND_TIMEOUT_MS <= MAX_WIFI_DURATION_MS,
              "Invalid command timeout");
}
