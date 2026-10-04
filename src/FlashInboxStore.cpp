#include "FlashInboxStore.h"
#include <LittleFS.h>
#include <esp_partition.h>
#include <esp_system.h>

bool FlashInboxStore::mount() {
    if (mounted_) return true;
    const auto* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        static_cast<esp_partition_subtype_t>(0x83), "inbox");
    if (!partition || !LittleFS.begin(false, "/littlefs", 4, "inbox")) return false;
    mounted_ = true;
    const uint32_t total = static_cast<uint32_t>(LittleFS.totalBytes());
    const uint32_t reserve = total / 50 > 65536 ? total / 50 : 65536;
    capacity_ = total > reserve ? total - reserve : 0;
    auto file = LittleFS.open("/messages.bin", "r");
    size_ = file ? static_cast<uint32_t>(file.size()) : 0;
    file.close();
    return true;
}
bool FlashInboxStore::initializeBlank() {
    if (mounted_) return true;
    const auto* partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
        static_cast<esp_partition_subtype_t>(0x83), "inbox");
    if (!partition) return false;
    uint8_t block[1024];
    for (uint32_t offset = 0; offset < partition->size; offset += sizeof(block)) {
        const size_t length = partition->size - offset < sizeof(block) ?
            partition->size - offset : sizeof(block);
        if (esp_partition_read(partition, offset, block, length) != ESP_OK) return false;
        for (size_t i = 0; i < length; ++i) if (block[i] != 0xff) return false;
    }
    if (!LittleFS.begin(true, "/littlefs", 4, "inbox")) return false;
    mounted_ = true;
    const uint32_t total = static_cast<uint32_t>(LittleFS.totalBytes());
    const uint32_t reserve = total / 50 > 65536 ? total / 50 : 65536;
    capacity_ = total > reserve ? total - reserve : 0; size_ = 0;
    return true;
}
bool FlashInboxStore::read(uint32_t offset, uint8_t* bytes, size_t length) {
    if (!mounted_ || !bytes || offset > size_ || length > size_ - offset) return false;
    if (!reader_) reader_ = LittleFS.open("/messages.bin", "r");
    return reader_ && reader_.seek(offset) && reader_.read(bytes, length) == length;
}
bool FlashInboxStore::write(uint32_t offset, const uint8_t* bytes, size_t length) {
    if (!mounted_ || !bytes || offset > size_ || length > UINT32_MAX - offset ||
        (offset + length > size_ && (offset > capacity_ || length > capacity_ - offset))) return false;
    reader_.close();
    auto file = LittleFS.open("/messages.bin", size_ ? "r+" : "w+");
    if (!file || !file.seek(offset)) { file.close(); return false; }
    const bool written = file.write(bytes, length) == length;
    if (written) file.flush();
    const uint32_t reported = static_cast<uint32_t>(file.size());
    file.close();
    if (!written || reported < offset + length) return false;
    if (reported > size_) size_ = reported;
    return true;
}
bool FlashInboxStore::fill(uint8_t* bytes, size_t length) {
    if (!bytes) return false;
    esp_fill_random(bytes, length); return true;
}
