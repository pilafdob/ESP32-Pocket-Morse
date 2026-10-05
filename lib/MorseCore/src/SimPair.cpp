#include "SimPair.h"
#include <string.h>
namespace morse {
SimPair::SimPair(const uint8_t root[32], uint32_t sessionA, uint32_t sessionB)
    : ta_(*this, 0), tb_(*this, 1), a_(ta_, sessionA), b_(tb_, sessionB) {
    ca_.begin(root, 0); cb_.begin(root, 1);
}
bool SimPair::Endpoint::send(const uint8_t* bytes, size_t length) {
    return pair_.send(device_, bytes, length);
}
void SimPair::button(unsigned device, Button button, bool down) {
    if (device < 2 && static_cast<unsigned>(button) < 2)
        down_[device][static_cast<unsigned>(button)] = down;
}
void SimPair::dropNext(unsigned from, PacketType type) {
    if (from < 2 && (type == PacketType::Text || type == PacketType::Ack))
        drops_[from][static_cast<unsigned>(type) - 1] = 1;
}
bool SimPair::send(unsigned from, const uint8_t* bytes, size_t length) {
    Packet packet;
    if (!deserialize(bytes, length, packet)) return false;
    uint8_t wire[SecureSize];
    const size_t wireLength = channel(from).seal(bytes, length, wire);
    if (!wireLength) return false;
    if (tamper_) { wire[SecureHeader] ^= 1; tamper_ = false; }
    const unsigned type = static_cast<unsigned>(packet.type);
    const bool oneShot = type <= 2 && drops_[from][type - 1];
    const bool dropped = offline_ || oneShot;
    if (oneShot) --drops_[from][type - 1];
    events_[eventWrite_] = {now_, packet.id, static_cast<uint8_t>(from),
                            static_cast<uint8_t>(packet.type), static_cast<uint8_t>(dropped)};
    eventWrite_ = (eventWrite_ + 1) % 64;
    if (eventWrite_ == eventRead_) eventRead_ = (eventRead_ + 1) % 64;
    if (dropped) return true;
    for (auto& flight : flights_) {
        if (flight.used) continue;
        memcpy(flight.bytes, wire, wireLength);
        flight.length = wireLength;
        flight.due = now_ + 80;
        flight.target = 1 - from;
        flight.used = true;
        return true;
    }
    return false;
}
void SimPair::tick(uint32_t now) {
    now_ = now;
    for (unsigned d = 0; d < 2; ++d)
        inputs_[d].sample(down_[d][0], down_[d][1], now_, app(d));
    for (auto& flight : flights_) {
        if (!flight.used || static_cast<int32_t>(now_ - flight.due) < 0) continue;
        // Release the slot before callback, but copy the payload before ACK allocation reuses it.
        const InFlight received = flight;
        flight.used = false;
        uint8_t plain[MaxPacket];
        const size_t length = channel(received.target).open(received.bytes, received.length, plain);
        if (length) app(received.target).receive(plain, length, now_);
        else {
            events_[eventWrite_] = {now_, 0, static_cast<uint8_t>(1 - received.target), 0, 0, 1};
            eventWrite_ = (eventWrite_ + 1) % 64;
            if (eventWrite_ == eventRead_) eventRead_ = (eventRead_ + 1) % 64;
        }
    }
    a_.tick(now_); b_.tick(now_);
    a_.setPeerSignalBars(offline_ ? 0 : 3);
    b_.setPeerSignalBars(offline_ ? 0 : 3);
}
bool SimPair::popEvent(Event& event) {
    if (eventRead_ == eventWrite_) return false;
    event = events_[eventRead_];
    eventRead_ = (eventRead_ + 1) % 64;
    return true;
}
}
