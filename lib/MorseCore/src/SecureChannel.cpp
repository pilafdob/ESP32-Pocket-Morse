#include "SecureChannel.h"
#include <monocypher.h>
#include <string.h>
namespace morse {
namespace {
void nonceFor(const uint8_t* header, uint8_t nonce[24]) {
    memset(nonce, 0, 24);
    memcpy(nonce, "MORSE-V2", 8);
    nonce[8] = header[3];
    memcpy(nonce + 16, header + 4, 8);
}
}
void deriveKey(const uint8_t root[32], const char* domain, uint8_t output[32]) {
    crypto_blake2b_keyed(output, 32, root, 32,
                        reinterpret_cast<const uint8_t*>(domain), strlen(domain));
}
SecureChannel::~SecureChannel() {
    crypto_wipe(txKey_, sizeof(txKey_)); crypto_wipe(rxKey_, sizeof(rxKey_));
}
void SecureChannel::begin(const uint8_t root[32], uint8_t role) {
    ready_ = false;
    if (!root || role > 1) return;
    bool nonzero = false;
    for (unsigned i = 0; i < 32; ++i) nonzero |= root[i] != 0;
    if (!nonzero) return;
    role_ = role;
    deriveKey(root, role == 0 ? "Morse-v2 A to B" : "Morse-v2 B to A", txKey_);
    deriveKey(root, role == 0 ? "Morse-v2 B to A" : "Morse-v2 A to B", rxKey_);
    ready_ = true;
}
size_t SecureChannel::seal(const uint8_t* plaintext, size_t length, uint8_t output[SecureSize]) {
    Packet packet;
    if (!ready_ || !output || !deserialize(plaintext, length, packet)) return 0;
    const auto counter = counters_.nextTx();
    if (!counter) return 0;
    output[0] = 'S'; output[1] = 'M'; output[2] = 2; output[3] = role_;
    for (unsigned i = 0; i < 8; ++i) output[4 + i] = counter >> (i * 8);
    uint8_t nonce[24], padded[MaxPacket] = {};
    nonceFor(output, nonce);
    memcpy(padded, plaintext, length);
    crypto_aead_lock(output + SecureHeader, output + SecureHeader + MaxPacket,
                     txKey_, nonce, output, SecureHeader, padded, sizeof(padded));
    crypto_wipe(padded, sizeof(padded));
    return SecureSize;
}
size_t SecureChannel::open(const uint8_t* wire, size_t length, uint8_t output[MaxPacket]) {
    if (!ready_ || !wire || !output || length != SecureSize || wire[0] != 'S' ||
        wire[1] != 'M' || wire[2] != 2 || wire[3] != 1 - role_) return 0;
    uint64_t counter = 0;
    for (unsigned i = 0; i < 8; ++i) counter |= uint64_t(wire[4 + i]) << (8 * i);
    if (!counter || counter <= counters_.lastRx()) return 0;
    uint8_t nonce[24], padded[MaxPacket]; nonceFor(wire, nonce);
    if (crypto_aead_unlock(padded, wire + SecureHeader + MaxPacket, rxKey_, nonce,
                          wire, SecureHeader, wire + SecureHeader, MaxPacket) != 0) return 0;
    const size_t plainLength = HeaderSize + padded[16];
    Packet packet;
    bool valid = plainLength <= MaxPacket && deserialize(padded, plainLength, packet);
    if (valid) for (size_t i = plainLength; i < MaxPacket; ++i) valid &= padded[i] == 0;
    const bool durable = packet.type == PacketType::Text || packet.type == PacketType::Ack;
    if (!valid || !counters_.commitRx(counter, durable)) { crypto_wipe(padded, sizeof(padded)); return 0; }
    memcpy(output, padded, plainLength);
    crypto_wipe(padded, sizeof(padded));
    return plainLength;
}
}
