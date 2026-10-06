#include "BatteryMonitor.h"
#include <Arduino.h>
#include "config.h"

void BoardBatteryInput::begin() {
    pinMode(config::BatteryEnablePin, OUTPUT);
    digitalWrite(config::BatteryEnablePin, LOW);
    analogReadResolution(12);
    analogSetPinAttenuation(config::BatterySensePin, ADC_11db);
}

void BoardBatteryInput::setEnabled(bool enabled) {
    digitalWrite(config::BatteryEnablePin, enabled ? HIGH : LOW);
}

uint16_t BoardBatteryInput::readMillivolts() {
    return static_cast<uint16_t>(analogReadMilliVolts(config::BatterySensePin));
}
