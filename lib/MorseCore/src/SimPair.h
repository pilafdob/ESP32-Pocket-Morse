#pragma once
#include "MorseCore.h"
#include "SecureChannel.h"

namespace morse {
// One simulated link, two independent applications. No radio emulation is claimed.
class SimPair {
public:
    struct Event {
        uint32_t at = 0, id = 0;
        uint8_t from = 0, type = 0, dropped = 0;
        uint8_t rejected = 0;
    };
    explicit SimPair(const uint8_t root[32], uint32_t sessionA = 101, uint32_t sessionB = 202);
    void tick(uint32_t now);
    void button(unsigned device, Button button, bool down);
    void dropNext(unsigned from, PacketType type);
    void tamperNext() { tamper_ = true; }
    void setOffline(bool offline) { offline_ = offline; }
    bool offline() const { return offline_; }
    App& app(unsigned device) { return device == 0 ? a_ : b_; }
    bool popEvent(Event& event);
    uint32_t now() const { return now_; }
private:
    class Endpoint : public ITransport {
    public:
        Endpoint(SimPair& pair, unsigned device) : pair_(pair), device_(device) {}
        bool send(const uint8_t* bytes, size_t length) override;
    private:
        SimPair& pair_;
        unsigned device_;
    } ta_, tb_;
    App a_, b_;
    InputManager inputs_[2];
    bool down_[2][2] = {};
    uint32_t now_ = 0;
    bool offline_ = false;
    bool tamper_ = false;
    MemoryCounters counters_[2];
    SecureChannel ca_{counters_[0]}, cb_{counters_[1]};
    SecureChannel& channel(unsigned d) { return d == 0 ? ca_ : cb_; }
    uint8_t drops_[2][2] = {};
    struct InFlight {
        uint8_t bytes[SecureSize] = {};
        size_t length = 0;
        uint32_t due = 0;
        unsigned target = 0;
        bool used = false;
    } flights_[32];
    Event events_[64];
    size_t eventRead_ = 0, eventWrite_ = 0;
    bool send(unsigned from, const uint8_t* bytes, size_t length);
};
}
