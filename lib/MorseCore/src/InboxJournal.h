#pragma once
#include "MorseCore.h"

namespace morse {
// Fixed-size encrypted records. Storage implementations must make a successful write
// durable and immediately readable, or report failure. No auto-format on corruption.
class IByteStore {
public:
    virtual ~IByteStore() = default;
    virtual uint32_t size() const = 0;
    virtual uint32_t capacity() const = 0;
    virtual bool read(uint32_t offset, uint8_t* bytes, size_t length) = 0;
    virtual bool write(uint32_t offset, const uint8_t* bytes, size_t length) = 0;
};
class IEntropy {
public:
    virtual ~IEntropy() = default;
    virtual bool fill(uint8_t* bytes, size_t length) = 0;
};
constexpr size_t InboxRecordSize = 144;
class InboxJournal : public IInbox {
public:
    InboxJournal(IByteStore& storage, IEntropy& entropy) : storage_(storage), entropy_(entropy) {}
    ~InboxJournal();
    bool begin(const uint8_t pairKey[32], uint8_t role);
    bool ready() const { return ready_; }
    StoreResult append(uint64_t session, uint32_t id, const char* text) override;
    uint32_t count() const override { return count_; }
    uint32_t unreadCount() const override { return unread_; }
    uint32_t remaining() const override;
    bool oldestUnread(InboxMessage& out) override;
    bool next(uint64_t after, InboxMessage& out) override;
    bool previous(uint64_t before, InboxMessage& out) override;
    uint32_t position(uint64_t ordinal) override;
    bool markRead(uint64_t ordinal) override;
    bool erase(uint64_t ordinal) override;
private:
    bool decode(uint32_t offset, InboxMessage& message, uint8_t& status);
    bool encode(const InboxMessage& message, uint8_t status, uint8_t record[InboxRecordSize]);
    bool writeRecord(uint32_t offset, const InboxMessage& message, uint8_t status);
    IByteStore& storage_;
    IEntropy& entropy_;
    uint8_t key_[32] = {};
    uint64_t nextOrdinal_ = 1;
    uint32_t count_ = 0, unread_ = 0, freeSlots_ = 0;
    uint32_t freeOffset_ = UINT32_MAX;
    struct Recent { uint64_t ordinal = 0, session = 0; uint32_t id = 0; } recent_[16] = {};
    void remember(const InboxMessage& message);
    bool ready_ = false;
};
}
