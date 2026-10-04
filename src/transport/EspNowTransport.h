#pragma once
#include <Arduino.h>
#include <esp_now.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include <MorseCore.h>
#include <SecureChannel.h>
#include "../PairStore.h"

class EspNowTransport : public morse::ITransport {
public:
    explicit EspNowTransport(PairStore& store) : store_(store), secure_(store) {}
    bool begin(uint8_t role, uint8_t channel);
    bool send(const uint8_t* bytes, size_t length) override;
    void poll(morse::App& app, uint32_t now);
    bool ready() const { return ready_; }
private:
    struct Received { uint8_t bytes[morse::SecureSize]; size_t length; };
    static void receiveCallback(const uint8_t* mac, const uint8_t* bytes, int length);
    static void sendCallback(const uint8_t* mac, esp_now_send_status_t status);
    static EspNowTransport* instance_;
    uint8_t peer_[6] = {};
    QueueHandle_t incoming_ = nullptr;
    QueueHandle_t outcomes_ = nullptr;
    bool ready_ = false;
    PairStore& store_;
    morse::SecureChannel secure_;
};
