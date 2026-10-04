#pragma once
#include <InboxJournal.h>
#include <FS.h>

class FlashInboxStore : public morse::IByteStore, public morse::IEntropy {
public:
    bool mount();
    // Only for a physically blank partition, on explicit provisioning command.
    bool initializeBlank();
    bool mounted() const { return mounted_; }
    uint32_t size() const override { return size_; }
    uint32_t capacity() const override { return capacity_; }
    bool read(uint32_t offset, uint8_t* bytes, size_t length) override;
    bool write(uint32_t offset, const uint8_t* bytes, size_t length) override;
    bool fill(uint8_t* bytes, size_t length) override;
private:
    bool mounted_ = false;
    uint32_t size_ = 0, capacity_ = 0;
    fs::File reader_;
};
