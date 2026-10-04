#pragma once
#include "MorseCore.h"

namespace morse {
// Fixed padding hides the message type and length inside the authenticated envelope.
constexpr size_t SecureHeader = 12;
constexpr size_t SecureSize = SecureHeader + MaxPacket + 16;
static_assert(SecureSize <= 250, "Must fit original ESP-NOW payload limit");
class CounterStore {
public:
    virtual ~CounterStore() = default;
    virtual uint64_t nextTx() = 0; // Persist/reserve BEFORE returning; 0 means fail closed.
    virtual uint64_t lastRx() const = 0;
    virtual bool commitRx(uint64_t counter, bool durable) = 0;
};
class MemoryCounters : public CounterStore {
public:
    uint64_t tx = 0, rx = 0;
    uint64_t nextTx() override { return tx == UINT64_MAX ? 0 : ++tx; }
    uint64_t lastRx() const override { return rx; }
    bool commitRx(uint64_t counter, bool) override { rx = counter; return true; }
};
void deriveKey(const uint8_t root[32], const char* domain, uint8_t output[32]);
class SecureChannel {
public:
    explicit SecureChannel(CounterStore& counters) : counters_(counters) {}
    ~SecureChannel();
    void begin(const uint8_t root[32], uint8_t role);
    size_t seal(const uint8_t* plaintext, size_t length, uint8_t output[SecureSize]);
    size_t open(const uint8_t* wire, size_t length, uint8_t output[MaxPacket]);
private:
    CounterStore& counters_;
    uint8_t txKey_[32] = {}, rxKey_[32] = {};
    uint8_t role_ = 0;
    bool ready_ = false;
};
}
