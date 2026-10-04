#pragma once
#include <stdint.h>

namespace config {
constexpr uint8_t DotPin = 0;
constexpr uint8_t DashPin = 35; // Input-only; no internal pull-up. Board has an external pull-up.
constexpr uint8_t Channel = 1; // Must match on both boards; no router required.
#ifdef DEVICE_A
constexpr const char* DeviceName = "A";
constexpr uint8_t Role = 0;
#elif defined(DEVICE_B)
constexpr const char* DeviceName = "B";
constexpr uint8_t Role = 1;
#else
#error Select DEVICE_A or DEVICE_B
#endif
}
