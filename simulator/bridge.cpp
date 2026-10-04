#include <emscripten/emscripten.h>
#include <SimPair.h>
#include <stdio.h>
#include <memory>
#include <monocypher.h>

EM_JS(void, random_key, (uint8_t* output), {
    HEAPU8.set(globalThis.crypto.getRandomValues(new Uint8Array(32)), output);
});

namespace {
std::unique_ptr<morse::SimPair> pair;
char stateJson[1400];
char frameJson[3000];
void escaped(const char* source, char* target, size_t capacity) {
    size_t used = 0;
    for (const char* p = source; *p && used + 2 < capacity; ++p) {
        if (*p == '"' || *p == '\\') target[used++] = '\\';
        target[used++] = *p;
    }
    target[used] = '\0';
}
class JsonScreen : public morse::IScreen {
public:
    size_t used = 0;
    void clear(uint16_t color) override {
        used = snprintf(frameJson, sizeof(frameJson), "[{\"clear\":%u}", color);
    }
    void text(int x, int y, const char* value, int size, uint16_t color) override {
        used += snprintf(frameJson + used, sizeof(frameJson) - used,
            ",{\"x\":%d,\"y\":%d,\"text\":\"", x, y);
        for (const char* p = value; *p && used + 8 < sizeof(frameJson); ++p) {
            if (*p == '"' || *p == '\\') frameJson[used++] = '\\';
            frameJson[used++] = *p;
        }
        used += snprintf(frameJson + used, sizeof(frameJson) - used,
            "\",\"size\":%d,\"color\":%u}", size, color);
    }
    void dial(int x, int y, uint8_t progress, uint16_t color) override {
        used += snprintf(frameJson + used, sizeof(frameJson) - used,
            ",{\"dial\":true,\"x\":%d,\"y\":%d,\"progress\":%u,\"color\":%u}",
            x, y, progress, color);
    }
};
}
extern "C" {
EMSCRIPTEN_KEEPALIVE void sim_reset() {
    uint8_t root[32]; random_key(root);
    pair.reset(new morse::SimPair(root));
    crypto_wipe(root, sizeof(root));
}
EMSCRIPTEN_KEEPALIVE void sim_tick(uint32_t now) { if (pair) pair->tick(now); }
EMSCRIPTEN_KEEPALIVE void sim_button(int device, int button, int down) {
    if (pair && device >= 0 && device < 2 && button >= 0 && button < 2)
        pair->button(device, static_cast<morse::Button>(button), down != 0);
}
EMSCRIPTEN_KEEPALIVE void sim_drop(int from, int type) {
    if (pair && from >= 0 && from < 2 && type >= 1 && type <= 2)
        pair->dropNext(from, static_cast<morse::PacketType>(type));
}
EMSCRIPTEN_KEEPALIVE void sim_offline(int offline) { if (pair) pair->setOffline(offline != 0); }
EMSCRIPTEN_KEEPALIVE void sim_tamper() { if (pair) pair->tamperNext(); }
EMSCRIPTEN_KEEPALIVE const char* sim_state(int device) {
    if (!pair || device < 0 || device > 1) return "{}";
    const auto& s = pair->app(device).state();
    char draft[morse::MaxText * 2 + 1], received[morse::MaxText * 2 + 1], lastSent[morse::MaxText * 2 + 1], notice[65];
    escaped(s.draft, draft, sizeof(draft)); escaped(s.received, received, sizeof(received));
    escaped(s.lastSent, lastSent, sizeof(lastSent)); escaped(s.notice, notice, sizeof(notice));
    snprintf(stateJson, sizeof(stateJson),
        "{\"sequence\":\"%s\",\"draft\":\"%s\",\"received\":\"%s\",\"lastSent\":\"%s\","
        "\"notice\":\"%s\",\"delivery\":\"%s\",\"attempts\":%u,\"messageId\":%lu,"
        "\"receivedCount\":%lu,\"duplicates\":%lu,\"mode\":\"%s\",\"connected\":%s,\"progress\":%u,\"dictionaryPage\":%u,\"unreadCount\":%lu,\"inboxCount\":%lu,\"inboxPosition\":%lu,\"inboxRemaining\":%lu,\"cursorVisible\":%s}",
        s.sequence, draft, received, lastSent, notice, morse::deliveryName(s.delivery),
        s.attempts, static_cast<unsigned long>(s.messageId),
        static_cast<unsigned long>(s.receivedCount), static_cast<unsigned long>(s.duplicates),
        morse::modeName(s.mode), s.peerConnected ? "true" : "false", s.progress, s.dictionaryPage,
        static_cast<unsigned long>(s.unreadCount), static_cast<unsigned long>(s.inboxCount),
        static_cast<unsigned long>(s.inboxPosition), static_cast<unsigned long>(s.inboxRemaining),
        s.cursorVisible ? "true" : "false");
    return stateJson;
}
EMSCRIPTEN_KEEPALIVE const char* sim_frame(int device, int landscape) {
    if (!pair || device < 0 || device > 1) return "[]";
    JsonScreen screen;
    morse::render(pair->app(device).state(), device == 0 ? "A" : "B", screen,
                  landscape ? morse::ScreenLayout::Landscape : morse::ScreenLayout::Portrait);
    snprintf(frameJson + screen.used, sizeof(frameJson) - screen.used, "]");
    return frameJson;
}
EMSCRIPTEN_KEEPALIVE const char* sim_event() {
    morse::SimPair::Event e;
    if (!pair || !pair->popEvent(e)) return "";
    snprintf(stateJson, sizeof(stateJson),
        "{\"at\":%lu,\"id\":%lu,\"from\":%u,\"type\":%u,\"dropped\":%s,\"rejected\":%s}",
        static_cast<unsigned long>(e.at), static_cast<unsigned long>(e.id),
        e.from, e.type, e.dropped ? "true" : "false", e.rejected ? "true" : "false");
    return stateJson;
}
}
