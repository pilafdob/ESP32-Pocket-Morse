#pragma once
#include <Preferences.h>
#include <SecureChannel.h>

class PairStore : public morse::CounterStore {
public:
    bool begin(uint8_t role);
    bool configured() const { return configured_; }
    const uint8_t* root() const { return record_.root; }
    const uint8_t* peer() const { return record_.peer; }
    bool provision(const uint8_t peer[6], const uint8_t root[32]);
    uint64_t nextTx() override;
    uint64_t lastRx() const override { return rx_; }
    bool commitRx(uint64_t counter, bool durable) override;
private:
    struct Record {
        uint32_t version = 2;
        uint8_t role = 0;
        uint8_t peer[6] = {};
        uint8_t root[32] = {};
        uint64_t txLimit = 0;
        uint64_t rx = 0;
    } record_;
    Preferences preferences_;
    bool available_ = false, configured_ = false;
    uint64_t tx_ = 0;
    uint64_t rx_ = 0;
    bool save(const Record& next);
};
