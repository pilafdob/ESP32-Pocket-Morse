#include "PairStore.h"
#include <string.h>
bool PairStore::begin(uint8_t role) {
    available_ = preferences_.begin("morse-pair", false);
    if (!available_) return false;
    Record stored;
    const size_t size = preferences_.getBytesLength("record");
    if (size == sizeof(stored) &&
        preferences_.getBytes("record", &stored, sizeof(stored)) == sizeof(stored) &&
        stored.version == 2 && stored.role == role) {
        record_ = stored; configured_ = true;
        // Skip the previous reservation on reboot, even after an unexpected power loss.
        tx_ = record_.txLimit;
        rx_ = record_.rx;
    } else {
        if (size != 0) { available_ = false; return false; }
        record_.role = role;
    }
    return true;
}
bool PairStore::save(const Record& next) {
    if (!available_ || preferences_.putBytes("record", &next, sizeof(next)) != sizeof(next)) return false;
    record_ = next;
    return true;
}
bool PairStore::provision(const uint8_t peer[6], const uint8_t root[32]) {
    // Do not reset counters or overwrite an existing key through routine provisioning.
    if (configured_ || !available_ || (peer[0] & 1)) return false;
    bool hasPeer = false, hasKey = false;
    for (unsigned i = 0; i < 6; ++i) hasPeer |= peer[i] != 0;
    for (unsigned i = 0; i < 32; ++i) hasKey |= root[i] != 0;
    if (!hasPeer || !hasKey) return false;
    Record next = record_;
    memcpy(next.peer, peer, 6); memcpy(next.root, root, 32);
    if (!save(next)) return false;
    configured_ = true;
    return true;
}
uint64_t PairStore::nextTx() {
    if (!configured_ || tx_ == UINT64_MAX) return 0;
    if (tx_ == record_.txLimit) {
        if (tx_ > UINT64_MAX - 256) return 0;
        Record next = record_; next.txLimit = tx_ + 256;
        if (!save(next)) return 0;
    }
    return ++tx_;
}
bool PairStore::commitRx(uint64_t counter, bool durable) {
    if (!configured_ || counter <= rx_) return false;
    if (!durable) { rx_ = counter; return true; }
    Record next = record_; next.rx = counter;
    if (!save(next)) return false;
    rx_ = counter;
    return true;
}
