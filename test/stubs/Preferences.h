#pragma once
#include <vector>
#include <cstring>
#include <cstddef>
// Minimal durable-store model for testing the production PairStore implementation.
class Preferences {
public:
    inline static std::vector<unsigned char> disk;
    inline static bool failWrites = false;
    inline static bool failOpen = false;
    bool begin(const char*, bool) { return !failOpen; }
    size_t getBytesLength(const char*) { return disk.size(); }
    size_t getBytes(const char*, void* dest, size_t n) {
        if (n != disk.size()) return 0;
        std::memcpy(dest, disk.data(), n); return n;
    }
    size_t putBytes(const char*, const void* src, size_t n) {
        if (failWrites) return 0;
        const auto* p = static_cast<const unsigned char*>(src);
        disk.assign(p, p + n); return n;
    }
};
