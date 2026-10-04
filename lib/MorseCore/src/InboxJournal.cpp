#include "InboxJournal.h"
#include "SecureChannel.h"
#include <monocypher.h>
#include <string.h>

namespace morse {
namespace {
constexpr size_t Header = 52, Tag = 16, Cipher = MaxText + 1;
static_assert(Header + Tag + Cipher <= InboxRecordSize, "Inbox record too small");
void put64(uint8_t* p, uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) p[i] = static_cast<uint8_t>(value >> (8 * i));
}
uint64_t get64(const uint8_t* p) {
    uint64_t value = 0;
    for (unsigned i = 0; i < 8; ++i) value |= uint64_t(p[i]) << (8 * i);
    return value;
}
}
InboxJournal::~InboxJournal() { crypto_wipe(key_, sizeof(key_)); }
void InboxJournal::remember(const InboxMessage& message) {
    size_t oldest = 0;
    for (size_t i = 1; i < 16; ++i)
        if (recent_[i].ordinal < recent_[oldest].ordinal) oldest = i;
    if (message.ordinal > recent_[oldest].ordinal)
        recent_[oldest] = {message.ordinal, message.session, message.id};
}
bool InboxJournal::begin(const uint8_t pairKey[32], uint8_t role) {
    ready_ = false; count_ = unread_ = freeSlots_ = 0; nextOrdinal_ = 1;
    freeOffset_ = UINT32_MAX; memset(recent_, 0, sizeof(recent_));
    if (!pairKey || role > 1 || storage_.size() % InboxRecordSize) return false;
    deriveKey(pairKey, role ? "Morse-v3 inbox B" : "Morse-v3 inbox A", key_);
    for (uint32_t offset = 0; offset < storage_.size(); offset += InboxRecordSize) {
        InboxMessage message; uint8_t status;
        if (!decode(offset, message, status)) return false;
        if (message.ordinal >= nextOrdinal_) {
            if (message.ordinal == UINT64_MAX) return false;
            nextOrdinal_ = message.ordinal + 1;
        }
        if (status != 3) { ++count_; unread_ += status == 1; }
        else { ++freeSlots_; if (freeOffset_ == UINT32_MAX) freeOffset_ = offset; }
        remember(message);
    }
    ready_ = true; return true;
}
uint32_t InboxJournal::remaining() const {
    if (!ready_) return 0;
    const uint32_t unused = storage_.capacity() > storage_.size() ?
        (storage_.capacity() - storage_.size()) / InboxRecordSize : 0;
    return freeSlots_ + unused;
}
bool InboxJournal::encode(const InboxMessage& message, uint8_t status, uint8_t record[InboxRecordSize]) {
    if (!message.ordinal || !message.session || !message.id || status < 1 || status > 3 ||
        strnlen(message.text, MaxText + 1) > MaxText) return false;
    memset(record, 0, InboxRecordSize);
    memcpy(record, "INB3", 4); record[4] = status;
    put64(record + 8, message.ordinal); put64(record + 16, message.session);
    for (unsigned i = 0; i < 4; ++i) record[24 + i] = message.id >> (i * 8);
    if (!entropy_.fill(record + 28, 24)) return false;
    uint8_t padded[Cipher] = {};
    memcpy(padded, message.text, strlen(message.text));
    crypto_aead_lock(record + Header + Tag, record + Header, key_, record + 28,
                     record, Header, padded, Cipher);
    crypto_wipe(padded, sizeof(padded));
    return true;
}
bool InboxJournal::decode(uint32_t offset, InboxMessage& message, uint8_t& status) {
    uint8_t record[InboxRecordSize];
    if (!storage_.read(offset, record, sizeof(record)) || memcmp(record, "INB3", 4)) return false;
    status = record[4];
    if (status < 1 || status > 3 || record[5] || record[6] || record[7]) return false;
    for (size_t i = Header + Tag + Cipher; i < InboxRecordSize; ++i) if (record[i]) return false;
    uint8_t padded[Cipher];
    if (crypto_aead_unlock(padded, record + Header, key_, record + 28,
                           record, Header, record + Header + Tag, Cipher) != 0) return false;
    const size_t length = strnlen(reinterpret_cast<const char*>(padded), Cipher);
    if (length > MaxText) { crypto_wipe(padded, sizeof(padded)); return false; }
    for (size_t i = length; i < Cipher; ++i) if (padded[i]) { crypto_wipe(padded, sizeof(padded)); return false; }
    message = InboxMessage{};
    message.ordinal = get64(record + 8); message.session = get64(record + 16);
    for (unsigned i = 0; i < 4; ++i) message.id |= uint32_t(record[24 + i]) << (8 * i);
    memcpy(message.text, padded, length); message.unread = status == 1;
    crypto_wipe(padded, sizeof(padded));
    return message.ordinal && message.session && message.id && (status == 3 || length);
}
bool InboxJournal::writeRecord(uint32_t offset, const InboxMessage& message, uint8_t status) {
    uint8_t record[InboxRecordSize];
    if (!encode(message, status, record)) return false;
    if (!storage_.write(offset, record, sizeof(record))) return false;
    InboxMessage check; uint8_t actual = 0;
    return decode(offset, check, actual) && actual == status &&
        check.ordinal == message.ordinal && check.session == message.session &&
        check.id == message.id && strcmp(check.text, message.text) == 0;
}
StoreResult InboxJournal::append(uint64_t session, uint32_t id, const char* text) {
    if (!ready_ || !session || !id || !text || !*text || strnlen(text, MaxText + 1) > MaxText)
        return StoreResult::Error;
    for (const auto& recent : recent_)
        if (recent.session == session && recent.id == id) return StoreResult::Duplicate;
    if (nextOrdinal_ == 0 || nextOrdinal_ == UINT64_MAX) return StoreResult::Full;
    const uint32_t offset = freeOffset_ != UINT32_MAX ? freeOffset_ : storage_.size();
    if (freeOffset_ == UINT32_MAX && (offset > storage_.capacity() ||
        storage_.capacity() - offset < InboxRecordSize)) return StoreResult::Full;
    InboxMessage message;
    message.ordinal = nextOrdinal_; message.session = session; message.id = id;
    memcpy(message.text, text, strlen(text) + 1);
    const uint32_t previousSize = storage_.size();
    if (!writeRecord(offset, message, 1)) {
        // A torn write may leave the journal inconsistent; stop all acknowledgements.
        if (offset == previousSize && storage_.size() == previousSize) return StoreResult::Full;
        ready_ = false; return StoreResult::Error;
    }
    if (freeOffset_ != UINT32_MAX) {
        --freeSlots_;
        freeOffset_ = UINT32_MAX;
        for (uint32_t candidate = offset + InboxRecordSize; candidate < storage_.size(); candidate += InboxRecordSize) {
            InboxMessage existing; uint8_t status;
            if (!decode(candidate, existing, status)) { ready_ = false; return StoreResult::Error; }
            if (status == 3) { freeOffset_ = candidate; break; }
        }
    }
    remember(message); ++nextOrdinal_; ++count_; ++unread_;
    return StoreResult::Stored;
}
bool InboxJournal::oldestUnread(InboxMessage& out) {
    if (!ready_) return false;
    bool found = false;
    for (uint32_t offset = 0; offset < storage_.size(); offset += InboxRecordSize) {
        InboxMessage message; uint8_t status;
        if (!decode(offset, message, status)) { ready_ = false; return false; }
        if (status == 1 && (!found || message.ordinal < out.ordinal)) { out = message; found = true; }
    }
    return found;
}
bool InboxJournal::next(uint64_t after, InboxMessage& out) {
    if (!ready_) return false;
    bool found = false;
    for (uint32_t offset = 0; offset < storage_.size(); offset += InboxRecordSize) {
        InboxMessage message; uint8_t status;
        if (!decode(offset, message, status)) { ready_ = false; return false; }
        if (status != 3 && message.ordinal > after && (!found || message.ordinal < out.ordinal))
            { out = message; found = true; }
    }
    return found;
}
bool InboxJournal::previous(uint64_t before, InboxMessage& out) {
    if (!ready_) return false;
    bool found = false;
    for (uint32_t offset = 0; offset < storage_.size(); offset += InboxRecordSize) {
        InboxMessage message; uint8_t status;
        if (!decode(offset, message, status)) { ready_ = false; return false; }
        if (status != 3 && message.ordinal < before && (!found || message.ordinal > out.ordinal))
            { out = message; found = true; }
    }
    return found;
}
uint32_t InboxJournal::position(uint64_t ordinal) {
    if (!ready_) return 0;
    uint32_t rank = 0;
    bool found = false;
    for (uint32_t offset = 0; offset < storage_.size(); offset += InboxRecordSize) {
        InboxMessage message; uint8_t status;
        if (!decode(offset, message, status)) { ready_ = false; return 0; }
        if (status != 3 && message.ordinal <= ordinal) ++rank;
        if (status != 3 && message.ordinal == ordinal) found = true;
    }
    return found ? rank : 0;
}
bool InboxJournal::markRead(uint64_t ordinal) {
    if (!ready_) return false;
    for (uint32_t offset = 0; offset < storage_.size(); offset += InboxRecordSize) {
        InboxMessage message; uint8_t status;
        if (!decode(offset, message, status)) { ready_ = false; return false; }
        if (message.ordinal == ordinal && status != 3) {
            if (status == 2) return true;
            if (!writeRecord(offset, message, 2)) { ready_ = false; return false; }
            --unread_; return true;
        }
    }
    return false;
}
bool InboxJournal::erase(uint64_t ordinal) {
    if (!ready_) return false;
    for (uint32_t offset = 0; offset < storage_.size(); offset += InboxRecordSize) {
        InboxMessage message; uint8_t status;
        if (!decode(offset, message, status)) { ready_ = false; return false; }
        if (message.ordinal == ordinal && status != 3) {
            memset(message.text, 0, sizeof(message.text));
            if (!writeRecord(offset, message, 3)) { ready_ = false; return false; }
            if (freeOffset_ == UINT32_MAX || offset < freeOffset_) freeOffset_ = offset;
            --count_; unread_ -= status == 1; ++freeSlots_; return true;
        }
    }
    return false;
}
}
