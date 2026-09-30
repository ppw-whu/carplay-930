#pragma once
#include <stdint.h>

// 与 Wi-Fi、PS3、Arduino 无关的统一运动接口。调用者负责并发互斥。
enum class ControlSource : uint8_t { None, PS3, WiFi };
enum class SubmitResult : uint8_t { Accepted, PS3Busy, RemoteInactive, StaleSession };

struct MotionState {
  ControlSource source;
  int left;
  int right;
  int auxLevel;
};

class MotionControl {
 public:
  explicit MotionControl(uint32_t ps3TimeoutMs = 500, int defaultPS3SpeedPercent = 80)
      : ps3TimeoutMs_(ps3TimeoutMs),
        defaultPS3SpeedPercent_(defaultPS3SpeedPercent >= 20 && defaultPS3SpeedPercent <= 100 &&
                               defaultPS3SpeedPercent % 20 == 0 ? defaultPS3SpeedPercent : 80),
        ps3SpeedPercent_(defaultPS3SpeedPercent_) {}

  // speed: -255~255，正值前进；turn: -255~255，正值右转。
  // PS3 必须先用组合键激活遥控模式；Wi-Fi 命令使用 durationMs 有效期。
  SubmitResult submit(ControlSource source, int speed, int turn,
                      uint32_t now, uint32_t durationMs, int auxLevel = -1,
                      uint32_t wifiSession = 0) {
    expire(now);
    if (source == ControlSource::PS3) {
      if (!remoteActive_) return SubmitResult::RemoteInactive;
      lastPS3Packet_ = now;
      seenPS3_ = true;
      ps3_ = {clamp(speed), clamp(turn), now, ps3TimeoutMs_, true};
      wifi_.valid = false;  // 接管时丢弃旧 Wi-Fi 命令，退出时不恢复旧动作。
      return SubmitResult::Accepted;
    }
    if (source == ControlSource::WiFi) {
      if (remoteActive_) return SubmitResult::PS3Busy;
      if (wifiSession != wifiSession_) return SubmitResult::StaleSession;
      wifi_ = {clamp(speed), clamp(turn), now, durationMs, durationMs > 0};
      // 显式设置档位，重复心跳不会重复加档；旧客户端省略时保持当前档位。
      if (auxLevel >= 0 && auxLevel <= 3) auxLevel_ = auxLevel;
      if (!wifi_.valid) disconnectWiFi();
      return SubmitResult::Accepted;
    }
    return SubmitResult::RemoteInactive;
  }

  // 每个手柄包调用：左摇杆纵轴 -> speed，右摇杆横轴 -> turn。
  // L1+R1 仅激活一次；释放组合键后仍保持模式。× 优先退出并停车。
  void receivePS3(int speed, int turn, bool activateCombo, bool stopPressed,
                  uint32_t now, bool auxPressed = false, bool speedPressed = false) {
    expire(now);
    lastPS3Packet_ = now;
    seenPS3_ = true;
    const bool stopEdge = stopPressed && !stopPressedLast_;
    stopPressedLast_ = stopPressed;
    if (stopPressed) {
      if (stopEdge) stop(ControlSource::PS3, now);
      return;
    }
    if (!activateCombo) comboReady_ = true;
    if (activateCombo && comboReady_) {
      comboReady_ = false;
      // 回中后再激活，避免接管时立刻运动；偏杆激活失败后需重新按组合键。
      if (!remoteActive_ && speed == 0 && turn == 0) {
        disconnectWiFi();
        remoteActive_ = true;
        auxLevel_ = 0;
        auxReady_ = false;
        resetPS3Speed();
      }
    }
    if (remoteActive_) {
      if (!speedPressed) speedReady_ = true;
      else if (speedReady_) {
        speedReady_ = false;
        ps3SpeedPercent_ = ps3SpeedPercent_ == 100 ? 20 : ps3SpeedPercent_ + 20;
      }
      // 进入遥控后先松开 △，每次按下只切换一次；一直按住不会连跳。
      if (!auxPressed) auxReady_ = true;
      else if (auxReady_) {
        auxReady_ = false;
        auxLevel_ = (auxLevel_ + 1) % 4;
      }
      // 右摇杆一旦离开死区，始终原地旋转，不和前进/后退叠加成弧线。
      submit(ControlSource::PS3, turn != 0 ? 0 : scalePS3(speed), scalePS3(turn), now, ps3TimeoutMs_);
    }
  }

  bool ps3ModeActive() const { return remoteActive_; }
  int ps3SpeedPercent() const { return ps3SpeedPercent_; }
  uint32_t wifiSession() const { return wifiSession_; }

  // 只有明确点击“切换到网页控制”才接管。会话号使旧网页的迟到请求失效。
  uint32_t claimWiFi(uint32_t now, uint32_t durationMs = 500) {
    stop();
    if (++sessionCounter_ == 0) ++sessionCounter_;
    wifiSession_ = sessionCounter_;
    wifi_ = {0, 0, now, durationMs, true};
    return wifiSession_;
  }

  bool releaseWiFi(uint32_t session = 0) {
    if (session != wifiSession_) return false;
    disconnectWiFi();
    return true;
  }

  // 两个来源均可停车并退出遥控；再次进入需重新按组合键。
  void stop(ControlSource source = ControlSource::None, uint32_t now = 0) {
    wifi_.valid = false;
    wifiSession_ = 0;
    ps3_.valid = false;
    remoteActive_ = false;
    comboReady_ = false;
    auxReady_ = false;
    auxLevel_ = 0;
    resetPS3Speed();
    if (source == ControlSource::PS3) {
      lastPS3Packet_ = now;
      seenPS3_ = true;
    }
  }

  void disconnectPS3() {
    resetPS3Speed();
    if (remoteActive_) auxLevel_ = 0;
    auxReady_ = false;
    ps3_.valid = false;
    remoteActive_ = false;
    comboReady_ = false;
    seenPS3_ = false;
    stopPressedLast_ = false;
  }

  void disconnectWiFi() {
    wifi_.valid = false;
    wifiSession_ = 0;
    if (!remoteActive_) auxLevel_ = 0;
  }

  bool ps3Online(uint32_t now) const {
    return seenPS3_ && uint32_t(now - lastPS3Packet_) < ps3TimeoutMs_;
  }

  MotionState update(uint32_t now) {
    expire(now);
    const Command* cmd = nullptr;
    ControlSource source = ControlSource::None;
    if (ps3_.valid) { cmd = &ps3_; source = ControlSource::PS3; }
    else if (wifi_.valid) { cmd = &wifi_; source = ControlSource::WiFi; }
    if (!cmd) return {ControlSource::None, 0, 0, 0};
    int left = cmd->speed + cmd->turn;
    int right = cmd->speed - cmd->turn;
    const int peak = maxInt(absInt(left), absInt(right));
    if (peak > 255) { left = left * 255 / peak; right = right * 255 / peak; }
    return {source, left, right, auxLevel_};
  }

 private:
  struct Command { int speed; int turn; uint32_t at; uint32_t ttl; bool valid; };
  Command ps3_ = {0, 0, 0, 0, false};
  Command wifi_ = {0, 0, 0, 0, false};
  uint32_t ps3TimeoutMs_;
  const int defaultPS3SpeedPercent_;
  int ps3SpeedPercent_;
  bool speedReady_ = false;
  uint32_t lastPS3Packet_ = 0;
  bool seenPS3_ = false;
  bool remoteActive_ = false;
  bool comboReady_ = false;
  bool auxReady_ = false;
  bool stopPressedLast_ = false;
  uint32_t wifiSession_ = 0;
  uint32_t sessionCounter_ = 0;
  int auxLevel_ = 0;

  static int absInt(int v) { return v < 0 ? -v : v; }
  static int maxInt(int a, int b) { return a > b ? a : b; }
  static int clamp(int v) { return v < -255 ? -255 : (v > 255 ? 255 : v); }
  int scalePS3(int value) const { return clamp(value) * ps3SpeedPercent_ / 100; }
  void resetPS3Speed() { ps3SpeedPercent_ = defaultPS3SpeedPercent_; speedReady_ = false; }
  void expire(uint32_t now) {
    if (seenPS3_ && !ps3Online(now)) {
      resetPS3Speed();
      if (remoteActive_) auxLevel_ = 0;
      auxReady_ = false;
      ps3_.valid = false;
      remoteActive_ = false;
      comboReady_ = false;
      seenPS3_ = false;
      stopPressedLast_ = false;
    }
    if (wifi_.valid && uint32_t(now - wifi_.at) >= wifi_.ttl) disconnectWiFi();
  }
};
