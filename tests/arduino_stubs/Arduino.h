#pragma once
#include <stdint.h>
#include "esp_arduino_version.h"
constexpr int LOW = 0;
constexpr int OUTPUT = 1;
static int testDuty[40] = {};
static bool testAttached[40] = {};
#if TEST_ESP32_MAJOR < 3
static int testChannelPin[16] = {};
static int testChannelDuty[16] = {};
#endif
inline void digitalWrite(uint8_t pin, int value) { testDuty[pin] = value; }
inline void pinMode(uint8_t, int) {}
inline void delayMicroseconds(unsigned) {}
inline int constrain(int value, int low, int high) {
  return value < low ? low : value > high ? high : value;
}
#if TEST_ESP32_MAJOR >= 3
inline bool ledcAttach(uint8_t pin, uint32_t frequency, uint8_t bits) {
  testAttached[pin] = frequency == 5000 && bits == 8;
  return testAttached[pin];
}
inline bool ledcWrite(uint8_t pin, uint32_t duty) {
  if (!testAttached[pin]) return false;
  testDuty[pin] = static_cast<int>(duty);
  return true;
}
inline void ledcDetach(uint8_t pin) { testAttached[pin] = false; }
#else
inline double ledcSetup(uint8_t channel, uint32_t frequency, uint8_t bits) {
  testChannelPin[channel] = -1;
  return frequency == 5000 && bits == 8 ? frequency : 0;
}
inline void ledcWrite(uint8_t channel, uint32_t duty) {
  testChannelDuty[channel] = static_cast<int>(duty);
  if (testChannelPin[channel] >= 0) testDuty[testChannelPin[channel]] = static_cast<int>(duty);
}
inline void ledcAttachPin(uint8_t pin, uint8_t channel) {
  testChannelPin[channel] = pin;
  testAttached[pin] = true;
  testDuty[pin] = testChannelDuty[channel];
}
inline void ledcDetachPin(uint8_t pin) { testAttached[pin] = false; }
#endif
