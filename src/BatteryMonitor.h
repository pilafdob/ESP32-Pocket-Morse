#pragma once

#include <MorseCore.h>

class BoardBatteryInput final : public morse::IBatteryInput {
public:
    void begin();
    void setEnabled(bool enabled) override;
    uint16_t readMillivolts() override;
};
