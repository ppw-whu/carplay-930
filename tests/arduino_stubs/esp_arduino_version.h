#pragma once
#define ESP_ARDUINO_VERSION_VAL(a, b, c) ((a) * 10000 + (b) * 100 + (c))
#ifndef TEST_ESP32_MAJOR
#define TEST_ESP32_MAJOR 3
#endif
#define ESP_ARDUINO_VERSION ESP_ARDUINO_VERSION_VAL(TEST_ESP32_MAJOR, 0, 0)
