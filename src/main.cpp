#include <Arduino.h>
#include <math.h>
#include <MorseCore.h>
#include "config.h"
#include <monocypher.h>
#include <InboxJournal.h>
#include <esp_system.h>
#ifdef MORSE_WOKWI
#include <SimPair.h>
// Wokwi runs one ESP32; the second App lives in the in-memory link, not a fake radio peer.
morse::SimPair* simulatedPair = nullptr;
#else
#include <TFT_eSPI.h>
#include <esp_system.h>
#include "transport/EspNowTransport.h"
#include "FlashInboxStore.h"
#include <WiFi.h>
PairStore pairStore;
FlashInboxStore inboxStorage;
morse::InboxJournal inboxJournal(inboxStorage, inboxStorage);
EspNowTransport radio(pairStore);
morse::App* realApp = nullptr;
morse::InputManager inputs;
TFT_eSPI tft;
TFT_eSprite frame(&tft);
class TftScreen : public morse::IScreen {
public:
    void clear(uint16_t color) override { frame.fillSprite(color); }
    void text(int x, int y, const char* value, int size, uint16_t color) override {
        frame.setTextFont(1); frame.setTextSize(size); frame.setTextColor(color);
        frame.setCursor(x, y); frame.print(value);
    }
    void dial(int x, int y, uint8_t progress, uint16_t color) override {
        frame.drawCircle(x, y, 8, TFT_DARKGREY);
        const int steps = (progress * 24 + 99) / 100;
        for (int i = 0; i < steps; ++i) {
            const float angle = (float(i) / 24.0f) * 6.2831853f - 1.5707963f;
            frame.drawPixel(x + int(roundf(8 * cosf(angle))), y + int(roundf(8 * sinf(angle))), color);
        }
    }
} screen;
bool framebufferReady = false;
#endif
uint32_t lastDraw = 0;
char lastStatus[384] = {};

#ifndef MORSE_WOKWI
void provisioning() {
    static char command[100] = {};
    static size_t used = 0;
    while (Serial.available()) {
        const char c = Serial.read();
        if (c == '\r') continue;
        if (c != '\n') {
            if (used < sizeof(command) - 1) command[used++] = c;
            continue;
        }
        command[used] = 0;
        if (strcmp(command, "INFO") == 0) {
            Serial.printf("MORSE_INFO %s %s %u\n", config::DeviceName,
                          WiFi.macAddress().c_str(), pairStore.configured() ? 1 : 0);
        } else if (strcmp(command, "INIT_INBOX") == 0) {
            const bool initialized = !pairStore.configured() &&
                (inboxStorage.mounted() || inboxStorage.initializeBlank());
            Serial.println(initialized ? "INBOX_OK" : "INBOX_REJECTED");
        } else if (strncmp(command, "PAIR ", 5) == 0 && used == 82) {
            uint8_t peer[6] = {}, root[32] = {};
            auto hex = [](char v) -> int { return v >= '0' && v <= '9' ? v - '0' :
                v >= 'a' && v <= 'f' ? v - 'a' + 10 : v >= 'A' && v <= 'F' ? v - 'A' + 10 : -1; };
            bool valid = command[17] == ' ';
            for (unsigned i = 0; i < 38; ++i) {
                const unsigned offset = i < 6 ? 5 + i * 2 : 18 + (i - 6) * 2;
                const int hi = hex(command[offset]), lo = hex(command[offset + 1]);
                if (hi < 0 || lo < 0) valid = false;
                if (i < 6) peer[i] = static_cast<uint8_t>((hi < 0 ? 0 : hi) * 16 + (lo < 0 ? 0 : lo));
                else root[i - 6] = static_cast<uint8_t>((hi < 0 ? 0 : hi) * 16 + (lo < 0 ? 0 : lo));
            }
            const bool saved = valid && inboxStorage.mounted() && pairStore.provision(peer, root);
            crypto_wipe(root, sizeof(root));
            const bool inboxReady = saved && inboxJournal.begin(pairStore.root(), config::Role);
            if (inboxReady) {
                realApp->attachInbox(inboxJournal);
                radio.begin(config::Role, config::Channel);
            }
            Serial.println(saved ? (inboxReady && radio.ready() ? "PAIR_OK" : "PAIR_STORED_NOT_READY") : "PAIR_REJECTED");
        } else Serial.println("COMMAND_REJECTED");
        crypto_wipe(command, sizeof(command)); used = 0;
    }
}
#endif

void setup() {
    Serial.begin(115200);
    pinMode(config::DotPin, INPUT_PULLUP);
    pinMode(config::DashPin, INPUT);
#ifndef MORSE_WOKWI
    WiFi.mode(WIFI_STA); // Enable RF entropy before generating the boot challenge/session.
    pairStore.begin(config::Role);
    inboxStorage.mount();
    if (pairStore.configured() && inboxStorage.mounted())
        inboxJournal.begin(pairStore.root(), config::Role);
    static morse::App application(radio, (uint64_t(esp_random()) << 32) | esp_random());
    realApp = &application;
    application.attachInbox(inboxJournal); // Never fall back to a volatile ACKed inbox on hardware.
    tft.init();
#ifdef MORSE_LANDSCAPE
    tft.setRotation(1);
#else
    tft.setRotation(0);
#endif
    tft.fillScreen(TFT_BLACK);
    frame.setColorDepth(16);
    framebufferReady = frame.createSprite(tft.width(), tft.height()) != nullptr;
    if (!framebufferReady) { tft.setTextColor(TFT_RED); tft.println("Framebuffer allocation failed"); }
    if (inboxJournal.ready()) radio.begin(config::Role, config::Channel);
    else Serial.println("INBOX NOT READY: no radio receipts. Provision or inspect storage.");
#else
    uint8_t demoKey[32]; esp_fill_random(demoKey, sizeof(demoKey));
    static morse::SimPair pair(demoKey);
    simulatedPair = &pair; crypto_wipe(demoKey, sizeof(demoKey));
    Serial.println("WOKWI: two-button controls; encrypted in-memory peer; no radio or TFT emulation.");
    Serial.println("Serial: t=drop next text; a=drop next ACK; x=toggle disconnected link.");
#endif
    Serial.printf("MORSE %s READY\n", config::DeviceName);
}
void loop() {
    const uint32_t now = millis();
#ifdef MORSE_WOKWI
    auto& pair = *simulatedPair;
    auto& app = pair.app(0);
    pair.button(0, morse::Button::Dot, digitalRead(config::DotPin) == LOW);
    pair.button(0, morse::Button::Dash, digitalRead(config::DashPin) == LOW);
    while (Serial.available()) {
        switch (Serial.read()) {
            case 't': pair.dropNext(0, morse::PacketType::Text); break;
            case 'a': pair.dropNext(1, morse::PacketType::Ack); break;
            case 'x': pair.setOffline(!pair.offline()); break;
        }
    }
    pair.tick(now);
#else
    auto& app = *realApp;
    provisioning();
    inputs.sample(digitalRead(config::DotPin) == LOW, digitalRead(config::DashPin) == LOW, now, app);
    radio.poll(app, now);
    app.tick(now);
#endif
    if (uint32_t(now - lastDraw) < 40) return;
    lastDraw = now;
    const auto& s = app.state();
    char status[384];
    snprintf(status, sizeof(status), "%s|%s|%s|%s|%s|%u|%lu|%lu|%s|%s|%u|%u|%u|%u|%lu|%lu|%lu|%lu|%llu",
        s.sequence, s.draft, s.received, s.lastSent, morse::deliveryName(s.delivery), s.attempts,
        static_cast<unsigned long>(s.receivedCount), static_cast<unsigned long>(s.messageId), s.notice, morse::modeName(s.mode), s.peerConnected, s.progress, s.dictionaryPage, s.cursorVisible,
        static_cast<unsigned long>(s.unreadCount), static_cast<unsigned long>(s.inboxCount),
        static_cast<unsigned long>(s.inboxPosition), static_cast<unsigned long>(s.inboxRemaining),
        static_cast<unsigned long long>(s.selectedOrdinal));
    if (strcmp(status, lastStatus) == 0) return;
    snprintf(lastStatus, sizeof(lastStatus), "%s", status);
    Serial.printf("MODE:%s STATUS:%s ATTEMPT:%u\n", morse::modeName(s.mode), morse::deliveryName(s.delivery), s.attempts);
#ifndef MORSE_WOKWI
    if (framebufferReady) {
        morse::render(s, config::DeviceName, screen,
#ifdef MORSE_LANDSCAPE
                      morse::ScreenLayout::Landscape
#else
                      morse::ScreenLayout::Portrait
#endif
        );
        if (!radio.ready()) screen.text(5, tft.height() - 10,
            inboxStorage.mounted() ? "PAIR VIA USB" : "INBOX ERROR", 1, TFT_RED);
        frame.pushSprite(0, 0);
    }
#endif
}
