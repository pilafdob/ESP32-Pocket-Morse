#include "EspNowTransport.h"
#include <WiFi.h>
#include <esp_wifi.h>
#include <string.h>
#include <monocypher.h>

EspNowTransport* EspNowTransport::instance_ = nullptr;
bool EspNowTransport::begin(uint8_t role, uint8_t channel) {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();
    Serial.printf("STA MAC: %s\n", WiFi.macAddress().c_str());
    if (!store_.configured()) {
        Serial.println("PAIR REQUIRED: use scripts/provision_pair.py over USB.");
        return false;
    }
    if (esp_wifi_set_channel(channel, WIFI_SECOND_CHAN_NONE) != ESP_OK) return false;
    memcpy(peer_, store_.peer(), 6);
    incoming_ = xQueueCreate(12, sizeof(Received));
    outcomes_ = xQueueCreate(12, sizeof(esp_now_send_status_t));
    if (!incoming_ || !outcomes_ || esp_now_init() != ESP_OK) return false;
    uint8_t pmk[32], lmk[32];
    morse::deriveKey(store_.root(), "Morse-v2 ESP-NOW PMK", pmk);
    morse::deriveKey(store_.root(), "Morse-v2 ESP-NOW LMK", lmk);
    if (esp_now_set_pmk(pmk) != ESP_OK) { crypto_wipe(pmk, 32); crypto_wipe(lmk, 32); return false; }
    crypto_wipe(pmk, 32);
    instance_ = this;
    if (esp_now_register_recv_cb(receiveCallback) != ESP_OK ||
        esp_now_register_send_cb(sendCallback) != ESP_OK) return false;
    esp_now_peer_info_t info = {};
    memcpy(info.peer_addr, peer_, 6);
    info.channel = channel;
    info.ifidx = WIFI_IF_STA;
    info.encrypt = true;
    memcpy(info.lmk, lmk, 16);
    crypto_wipe(lmk, 32);
    secure_.begin(store_.root(), role);
    ready_ = esp_now_add_peer(&info) == ESP_OK;
    Serial.printf("ESP-NOW %s; channel %u\n", ready_ ? "READY" : "INIT FAILED", channel);
    return ready_;
}
bool EspNowTransport::send(const uint8_t* bytes, size_t length) {
    if (!ready_) return false;
    uint8_t wire[morse::SecureSize];
    const auto wireLength = secure_.seal(bytes, length, wire);
    return wireLength && esp_now_send(peer_, wire, wireLength) == ESP_OK;
}
void EspNowTransport::receiveCallback(const uint8_t* mac, const uint8_t* bytes, int length) {
    auto* self = instance_;
    if (!self || !mac || !bytes || memcmp(mac, self->peer_, 6) != 0 ||
        length != static_cast<int>(morse::SecureSize)) return;
    Received message = {};
    memcpy(message.bytes, bytes, length);
    message.length = static_cast<size_t>(length);
    // WiFi task: queue copies only. No UI, application state, or blocking work here.
    xQueueSend(self->incoming_, &message, 0);
}
void EspNowTransport::sendCallback(const uint8_t*, esp_now_send_status_t status) {
    if (instance_) xQueueSend(instance_->outcomes_, &status, 0);
}
void EspNowTransport::poll(morse::App& app, uint32_t now) {
    if (!ready_) return;
    Received message;
    while (xQueueReceive(incoming_, &message, 0) == pdTRUE) {
        uint8_t plain[morse::MaxPacket];
        const auto length = secure_.open(message.bytes, message.length, plain);
        if (length) app.receive(plain, length, now);
        else Serial.println("SECURE PACKET REJECTED");
        crypto_wipe(plain, sizeof(plain));
    }
    esp_now_send_status_t status;
    while (xQueueReceive(outcomes_, &status, 0) == pdTRUE)
        if (status != ESP_NOW_SEND_SUCCESS) Serial.println("RADIO SEND FAILED (awaiting application retry)");
}
