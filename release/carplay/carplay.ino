/*
 * Maker-ESP32 四轮差速小车：PS3 / 固定 IP HTTP -> 统一接口 -> 四轮电机。
 * Arduino IDE 打开本文件；Wi-Fi、配对码和轮子方向填写在 CarConfig.h。
 * 依赖厂家 emakefun_esp32_ps3 2.0.1，推荐 ESP32 开发板包 3.3.2-cn。
 * 开发板 ESP32 Dev Module，Partition Scheme 选择 Huge APP (3MB No OTA)。
 */
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Ps3Controller.h>
#include "CarConfig.h"
#include "MotionControl.h"
#include "MotorDriver.h"
#include "ControlPage.h"

#if !defined(CONFIG_IDF_TARGET_ESP32)
#error "Please select a classic ESP32 board (ESP32 Dev Module), not S3/C3/S2."
#endif

WebServer server(80);
MotorDriver motors;
MotionControl motion(CarConfig::COMMAND_TIMEOUT_MS, CarConfig::PS3_DEFAULT_SPEED_PERCENT);
portMUX_TYPE motionMux = portMUX_INITIALIZER_UNLOCKED;
bool wifiConfigured = false;
bool lastWiFiConnected = false;
bool ps3Started = false;

// HTTP 通过这里提交；手柄经 receivePS3 进入同一 MotionControl::submit 接口。
SubmitResult submitMotion(ControlSource source, int speed, int turn,
                          uint32_t durationMs, int auxLevel = -1, uint32_t session = 0) {
  portENTER_CRITICAL(&motionMux);
  const SubmitResult result = motion.submit(source, speed, turn, millis(), durationMs, auxLevel, session);
  portEXIT_CRITICAL(&motionMux);
  return result;
}

void stopMotion(ControlSource source = ControlSource::None) {
  portENTER_CRITICAL(&motionMux);
  motion.stop(source, millis());
  portEXIT_CRITICAL(&motionMux);
}

// 独立任务刷新电机并执行超时检查，HTTP 客户端慢/卡住也不会延迟看门狗。
void motorTask(void*) {
  TickType_t next = xTaskGetTickCount();
  for (;;) {
    const bool online = wifiConfigured && WiFi.softAPgetStationNum() > 0;
    portENTER_CRITICAL(&motionMux);
    if (!online) motion.disconnectWiFi();
    const MotionState state = motion.update(millis());
    portEXIT_CRITICAL(&motionMux);
    motors.setWheels(state.left, state.right);
    motors.setAuxLevel(state.auxLevel);
    vTaskDelayUntil(&next, pdMS_TO_TICKS(CarConfig::MOTOR_PERIOD_MS));
  }
}

int joystickSpeed(int raw) {
  const int magnitude = abs(raw);
  if (magnitude <= CarConfig::JOYSTICK_DEADZONE) return 0;
  const int maxRaw = raw < 0 ? 128 : 127;
  // 先归一化到完整PWM范围，档位比例由统一控制层应用一次。
  const int value = (magnitude - CarConfig::JOYSTICK_DEADZONE) * 255 /
                    (maxRaw - CarConfig::JOYSTICK_DEADZONE);
  return raw < 0 ? -value : value;
}

// 厂家库每个有效数据包调用一次，即使摇杆位置不变也能刷新期限。
void onPS3Data() {
  const int speed = -joystickSpeed(Ps3.data.analog.stick.ly);
  const int turn = joystickSpeed(Ps3.data.analog.stick.rx);
  const bool activateCombo = Ps3.data.button.l1 && Ps3.data.button.r1;
  portENTER_CRITICAL(&motionMux);
  motion.receivePS3(speed, turn, activateCombo, Ps3.data.button.cross, millis(),
                    Ps3.data.button.triangle, Ps3.data.button.square);
  portEXIT_CRITICAL(&motionMux);
}

void onPS3Disconnect() {
  portENTER_CRITICAL(&motionMux);
  motion.disconnectPS3();
  portEXIT_CRITICAL(&motionMux);
  // 部分厂家库不会触发断连回调，motorTask 仍会根据最后数据包超时停车。
}

bool validMac(const char* value) {
  if (strlen(value) != 17) return false;
  for (int i = 0; i < 17; ++i) {
    if (i % 3 == 2) { if (value[i] != ':') return false; }
    else if (!isxdigit(static_cast<unsigned char>(value[i]))) return false;
  }
  return true;
}

// 严格解析；缺少参数、空串、字母和越界值都不能变成有效运动命令。
bool readNumber(const char* key, int low, int high, int& value) {
  if (!server.hasArg(key)) return false;
  const String text = server.arg(key);
  if (text.length() == 0 || text.length() > 6) return false;
  unsigned int start = text[0] == '-' ? 1 : 0;
  if (start == text.length()) return false;
  for (unsigned int i = start; i < text.length(); ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
  }
  const long n = text.toInt();
  if (n < low || n > high) return false;
  value = static_cast<int>(n);
  return true;
}

// 省略时保留旧接口的兼容模式；显式会话号必须为正 uint32_t。
bool readSession(uint32_t& value) {
  value = 0;
  if (!server.hasArg("session")) return true;
  const String text = server.arg("session");
  if (text.length() == 0 || text.length() > 10) return false;
  for (unsigned int i = 0; i < text.length(); ++i) {
    if (text[i] < '0' || text[i] > '9') return false;
    const uint32_t digit = text[i] - '0';
    if (value > (UINT32_MAX - digit) / 10) return false;
    value = value * 10 + digit;
  }
  return value != 0;
}

void sendJson(int status, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(status, "application/json; charset=utf-8", body);
}

bool controlMethodAllowed() {
  if (server.method() == HTTP_GET || server.method() == HTTP_POST) return true;
  server.sendHeader("Allow", "GET, POST");
  sendJson(405, "{\"error\":\"use GET or POST\"}");
  return false;
}

void handleMove() {
  if (!controlMethodAllowed()) return;
  int speed, turn;
  int duration = CarConfig::COMMAND_TIMEOUT_MS;
  int auxLevel = -1;
  uint32_t session = 0;
  if (!readNumber("speed", -255, 255, speed) || !readNumber("turn", -255, 255, turn) ||
      (server.hasArg("ms") && !readNumber("ms", 100, CarConfig::MAX_WIFI_DURATION_MS, duration)) ||
      (server.hasArg("aux") && !readNumber("aux", 0, 3, auxLevel)) || !readSession(session)) {
    sendJson(400, "{\"error\":\"speed/turn: -255..255; ms: 100..2000; aux: 0..3; session: positive uint32\"}");
    return;
  }
  const SubmitResult result = submitMotion(ControlSource::WiFi, speed, turn, duration, auxLevel, session);
  if (result == SubmitResult::PS3Busy) {
    sendJson(409, "{\"code\":\"ps3_busy\",\"error\":\"PS3 controls the car; explicitly POST /claim to switch\"}");
    return;
  }
  if (result == SubmitResult::StaleSession) {
    sendJson(409, "{\"code\":\"control_changed\",\"error\":\"session expired or control changed; explicitly POST /claim again\"}");
    return;
  }
  sendJson(200, String("{\"ok\":true,\"source\":\"wifi\",\"valid_ms\":") + duration + "}");
}

void handleClaim() {
  if (server.method() != HTTP_POST) {
    server.sendHeader("Allow", "POST");
    sendJson(405, "{\"error\":\"use POST\"}");
    return;
  }
  portENTER_CRITICAL(&motionMux);
  const uint32_t session = motion.claimWiFi(millis(), CarConfig::COMMAND_TIMEOUT_MS);
  portEXIT_CRITICAL(&motionMux);
  sendJson(200, String("{\"ok\":true,\"session\":") + session +
           ",\"valid_ms\":" + CarConfig::COMMAND_TIMEOUT_MS + "}");
}

// 网页离开/通信失败时仅释放网页自己的输出，避免迟到的停车请求抢停手柄。
// 明确点击“停车”仍使用 /stop，关闭所有电机。
void handleRelease() {
  if (!controlMethodAllowed()) return;
  uint32_t session = 0;
  if (!readSession(session)) {
    sendJson(400, "{\"error\":\"session must be a positive uint32\"}");
    return;
  }
  portENTER_CRITICAL(&motionMux);
  motion.update(millis());
  const bool released = motion.releaseWiFi(session);
  portEXIT_CRITICAL(&motionMux);
  if (!released) {
    sendJson(409, "{\"code\":\"control_changed\",\"error\":\"session expired or control changed\"}");
    return;
  }
  sendJson(200, "{\"ok\":true,\"released\":true}");
}

void handleStop() {
  if (!controlMethodAllowed()) return;
  stopMotion();
  sendJson(200, "{\"ok\":true,\"stopped\":true}");
}

void handleStatus() {
  portENTER_CRITICAL(&motionMux);
  const uint32_t now = millis();
  const MotionState state = motion.update(now);
  const bool ps3Online = motion.ps3Online(now);
  const bool remoteActive = motion.ps3ModeActive();
  const int ps3SpeedPercent = motion.ps3SpeedPercent();
  const uint32_t wifiSession = motion.wifiSession();
  portEXIT_CRITICAL(&motionMux);
  const char* source = state.source == ControlSource::PS3 ? "ps3" :
                       state.source == ControlSource::WiFi ? "wifi" : "none";
  String body = "{\"source\":\"";
  body += source;
  body += "\",\"left\":"; body += state.left;
  body += ",\"right\":"; body += state.right;
  body += ",\"ps3_online\":"; body += ps3Online ? "true" : "false";
  body += ",\"remote_mode\":"; body += remoteActive ? "true" : "false";
  body += ",\"ps3_speed_percent\":"; body += ps3SpeedPercent;
  body += ",\"wifi_session\":"; body += wifiSession;
  body += ",\"aux_level\":"; body += state.auxLevel;
  body += ",\"aux_pwm\":"; body += CarConfig::AUX_MOTOR_PWM[state.auxLevel];
  body += ",\"ip\":\""; body += WiFi.softAPIP().toString(); body += "\"}";
  sendJson(200, body);
}

void setupWiFi() {
  server.on("/", HTTP_GET, []() {
    server.sendHeader("Cache-Control", "no-store");
    server.send_P(200, "text/html; charset=utf-8", CONTROL_PAGE);
  });
  server.on("/move", HTTP_ANY, handleMove);
  server.on("/claim", HTTP_ANY, handleClaim);
  server.on("/stop", HTTP_ANY, handleStop);
  server.on("/release", HTTP_ANY, handleRelease);
  server.on("/status", HTTP_GET, handleStatus);
  server.onNotFound([]() { sendJson(404, "{\"error\":\"not found\"}"); });

  if (strlen(CarConfig::WIFI_SSID) == 0 || strlen(CarConfig::WIFI_PASSWORD) < 8 ||
      strlen(CarConfig::WIFI_PASSWORD) > 63) {
    Serial.println("Wi-Fi disabled: set AP name and an 8..63 character password in CarConfig.h.");
    return;
  }
  WiFi.mode(WIFI_AP);
  const auto* ip = CarConfig::LOCAL_IP;
  const auto* mask = CarConfig::SUBNET;
  const IPAddress address(ip[0], ip[1], ip[2], ip[3]);
  if (!WiFi.softAPConfig(address, address, IPAddress(mask[0], mask[1], mask[2], mask[3])) ||
      !WiFi.softAP(CarConfig::WIFI_SSID, CarConfig::WIFI_PASSWORD)) {
    Serial.println("Wi-Fi hotspot initialization failed; Wi-Fi control disabled.");
    WiFi.mode(WIFI_OFF);
    return;
  }
  wifiConfigured = true;
  server.begin();
  Serial.print("Connect to car Wi-Fi: "); Serial.println(CarConfig::WIFI_SSID);
  Serial.print("Car API: http://"); Serial.println(WiFi.softAPIP());
}

void setup() {
  // 尽早关闭高电平导通模块；复位到 setup 前仍需模块自身的输入下拉。
  digitalWrite(CarConfig::AUX_MOTOR_PIN, LOW);
  pinMode(CarConfig::AUX_MOTOR_PIN, OUTPUT);
  Serial.begin(115200);
  if (!motors.begin()) {
    Serial.println("Motor PWM initialization failed. Stopped.");
    for (;;) delay(1000);
  }
  // 蓝牙库会设置基址 MAC，因此必须早于 WiFi.mode / WiFi.softAP 初始化。
  if (validMac(CarConfig::PS3_HOST_MAC)) {
    Ps3.attach(onPS3Data);
    Ps3.attachOnDisconnect(onPS3Disconnect);
    ps3Started = Ps3.begin(CarConfig::PS3_HOST_MAC);
    Serial.println(ps3Started ? "PS3 ready: center sticks, press L1+R1 to activate; Cross exits." :
                               "PS3 initialization failed; Wi-Fi can still be used.");
  } else {
    Serial.println("PS3 disabled: fill the controller pairing code in PS3_HOST_MAC.");
  }
  setupWiFi();
  if (xTaskCreatePinnedToCore(motorTask, "car-motors", 3072, nullptr, 2, nullptr, 1) != pdPASS) {
    motors.stop();
    Serial.println("Motor task creation failed. Stopped.");
    for (;;) delay(1000);
  }
}

void loop() {
  if (wifiConfigured) {
    const bool connected = WiFi.softAPgetStationNum() > 0;
    if (connected && !lastWiFiConnected) {
      Serial.println("Wi-Fi client connected to car hotspot.");
    } else if (!connected && lastWiFiConnected) {
      portENTER_CRITICAL(&motionMux);
      motion.disconnectWiFi();
      portEXIT_CRITICAL(&motionMux);
      Serial.println("All Wi-Fi clients disconnected. Wi-Fi movement stopped.");
    }
    lastWiFiConnected = connected;
    server.handleClient();
  }
  delay(2);
}
