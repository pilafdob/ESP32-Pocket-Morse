#include "MorseCore.h"
#include <stdio.h>
#include <string.h>
#include <algorithm>

namespace morse {
namespace {
const char Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,:?'-/()\"=+@";
const char* const Codes[] = {
    ".-", "-...", "-.-.", "-..", ".", "..-.", "--.", "....", "..", ".---",
    "-.-", ".-..", "--", "-.", "---", ".--.", "--.-", ".-.", "...", "-",
    "..-", "...-", ".--", "-..-", "-.--", "--..", "-----", ".----", "..---",
    "...--", "....-", ".....", "-....", "--...", "---..", "----.",
    ".-.-.-", "--..--", "---...", "..--..", ".----.", "-....-",
    "-..-.", "-.--.", "-.--.-", ".-..-.", "-...-", ".-.-.", "--.-."
};
bool validText(const char* value, size_t length) {
    for (size_t i = 0; i < length; ++i)
        if (!(value[i] == ' ' || encode(value[i]))) return false;
    return true;
}
void put32(uint8_t* p, uint32_t n) {
    for (unsigned i = 0; i < 4; ++i) p[i] = static_cast<uint8_t>(n >> (8 * i));
}
uint32_t get32(const uint8_t* p) {
    uint32_t n = 0;
    for (unsigned i = 0; i < 4; ++i) n |= uint32_t(p[i]) << (8 * i);
    return n;
}
void twoLines(IScreen& screen, const char* value, int y, uint16_t color) {
    char line[39] = {};
    const size_t length = strlen(value);
    const size_t first = length < 38 ? length : 38;
    memcpy(line, value, first);
    screen.text(6, y, length ? line : "(empty)", 1, color);
    if (length > first) screen.text(6, y + 10, value + first, 1, color);
}
}
char decode(const char* sequence) {
    if (!sequence) return '\0';
    if (!sequence[0]) return '\0';
    for (size_t i = 0; i < sizeof(Alphabet) - 1; ++i)
        if (Codes[i][0] && strcmp(sequence, Codes[i]) == 0) return Alphabet[i];
    return '\0';
}
const char* encode(char character) {
    const char* p = character ? strchr(Alphabet, character) : nullptr;
    return p && *Codes[p - Alphabet] ? Codes[p - Alphabet] : nullptr;
}
StoreResult MemoryInbox::append(uint64_t session, uint32_t id, const char* value) {
    for (const auto& message : messages_)
        if (message.session == session && message.id == id) return StoreResult::Duplicate;
    if (messages_.size() >= capacity_ || nextOrdinal_ == 0) return StoreResult::Full;
    InboxMessage message;
    message.ordinal = nextOrdinal_++; message.session = session; message.id = id;
    memcpy(message.text, value, strnlen(value, MaxText) + 1);
    messages_.push_back(message);
    return StoreResult::Stored;
}
uint32_t MemoryInbox::count() const { return static_cast<uint32_t>(messages_.size()); }
uint32_t MemoryInbox::unreadCount() const {
    uint32_t n = 0; for (const auto& message : messages_) n += message.unread; return n;
}
uint32_t MemoryInbox::remaining() const {
    return static_cast<uint32_t>(capacity_ > messages_.size() ? capacity_ - messages_.size() : 0);
}
bool MemoryInbox::oldestUnread(InboxMessage& out) {
    for (const auto& message : messages_) if (message.unread) { out = message; return true; }
    return false;
}
bool MemoryInbox::next(uint64_t after, InboxMessage& out) {
    bool found = false;
    for (const auto& message : messages_) if (message.ordinal > after &&
        (!found || message.ordinal < out.ordinal)) { out = message; found = true; }
    return found;
}
bool MemoryInbox::previous(uint64_t before, InboxMessage& out) {
    bool found = false;
    for (const auto& message : messages_) if (message.ordinal < before &&
        (!found || message.ordinal > out.ordinal)) { out = message; found = true; }
    return found;
}
uint32_t MemoryInbox::position(uint64_t ordinal) {
    for (size_t i = 0; i < messages_.size(); ++i)
        if (messages_[i].ordinal == ordinal) return static_cast<uint32_t>(i + 1);
    return 0;
}
bool MemoryInbox::markRead(uint64_t ordinal) {
    for (auto& message : messages_) if (message.ordinal == ordinal) { message.unread = false; return true; }
    return false;
}
bool MemoryInbox::erase(uint64_t ordinal) {
    const auto found = std::find_if(messages_.begin(), messages_.end(),
        [ordinal](const InboxMessage& message) { return message.ordinal == ordinal; });
    if (found == messages_.end()) return false;
    messages_.erase(found); return true;
}
size_t dictionaryCount() { return sizeof(Alphabet) - 1; }
uint8_t signalBarsFromRssi(int8_t rssi) {
    if (rssi >= -55) return 3;
    if (rssi >= -70) return 2;
    if (rssi >= -85) return 1;
    return 0;
}
char dictionarySymbol(size_t index) {
    if (index >= sizeof(Alphabet) - 1) return '\0';
    const char symbol = Alphabet[index];
    return symbol;
}
const char* dictionaryCode(size_t index) {
    const char symbol = dictionarySymbol(index);
    return symbol ? Codes[index] : nullptr;
}
size_t serialize(const Packet& p, uint8_t* bytes, size_t capacity) {
    const size_t length = strnlen(p.text, MaxText + 1);
    if (!bytes || !p.session || !p.id || length > MaxText ||
        (p.type != PacketType::Text && p.type != PacketType::Ack && p.type != PacketType::Ping && p.type != PacketType::Pong) ||
        (p.type == PacketType::Text && (!length || !validText(p.text, length))) ||
        (p.type != PacketType::Text && length) || capacity < HeaderSize + length) return 0;
    bytes[0] = 'M'; bytes[1] = 'C'; bytes[2] = 2;
    bytes[3] = static_cast<uint8_t>(p.type);
    put32(bytes + 4, static_cast<uint32_t>(p.session));
    put32(bytes + 8, static_cast<uint32_t>(p.session >> 32)); put32(bytes + 12, p.id);
    bytes[16] = static_cast<uint8_t>(length);
    // Binary Morse: a leading 1 marks length, followed by dot=0/dash=1.
    // Space is 0. This carries Morse symbols, not ASCII character codes.
    for (size_t i = 0; i < length; ++i) {
        uint8_t code = 0;
        if (p.text[i] != ' ') {
            code = 1;
            for (const char* s = encode(p.text[i]); *s; ++s)
                code = static_cast<uint8_t>((code << 1) | (*s == '-'));
        }
        bytes[HeaderSize + i] = code;
    }
    return HeaderSize + length;
}
bool deserialize(const uint8_t* bytes, size_t length, Packet& p) {
    if (!bytes || length < HeaderSize || length > MaxPacket || bytes[0] != 'M' ||
        bytes[1] != 'C' || bytes[2] != 2 || length != HeaderSize + bytes[16]) return false;
    Packet candidate;
    candidate.type = static_cast<PacketType>(bytes[3]);
    candidate.session = uint64_t(get32(bytes + 4)) | (uint64_t(get32(bytes + 8)) << 32);
    candidate.id = get32(bytes + 12);
    for (size_t i = 0; i < bytes[16]; ++i) {
        uint8_t code = bytes[HeaderSize + i];
        if (!code) { candidate.text[i] = ' '; continue; }
        if (code == 1 || code > 127) return false;
        char sequence[MaxSequence + 1] = {};
        unsigned bits = 0;
        for (uint8_t n = code; n > 1; n >>= 1) ++bits;
        for (unsigned b = 0; b < bits; ++b)
            sequence[b] = code & (1 << (bits - b - 1)) ? '-' : '.';
        candidate.text[i] = decode(sequence);
        if (!candidate.text[i]) return false;
    }
    uint8_t check[MaxPacket];
    if (serialize(candidate, check, sizeof(check)) != length || memcmp(check, bytes, length)) return false;
    p = candidate;
    return true;
}
const char* deliveryName(Delivery d) {
    switch (d) {
        case Delivery::Idle: return "READY";
        case Delivery::Sending: return "SENDING";
        case Delivery::Delivered: return "DELIVERED";
        case Delivery::Failed: return "FAILED";
    }
    return "UNKNOWN";
}
const char* modeName(Mode mode) {
    switch (mode) {
        case Mode::Compose: return "COMPOSE";
        case Mode::Incoming: return "INCOMING";
        case Mode::Reading: return "READING";
        case Mode::Dictionary: return "DICTIONARY";
    }
    return "UNKNOWN";
}
App::App(ITransport& transport, uint64_t session)
    : transport_(transport), session_(session ? session : 1) { refreshInbox(); }
void App::attachInbox(IInbox& inbox) {
    inbox_ = &inbox;
    refreshInbox();
}
void App::refreshInbox() {
    state_.inboxCount = inbox_->count(); state_.unreadCount = inbox_->unreadCount();
    state_.inboxRemaining = inbox_->remaining();
}
bool App::openInbox() {
    InboxMessage message;
    if (inbox_->oldestUnread(message)) { showMessage(message); return true; }
    if (inbox_->next(0, message)) { showMessage(message); return true; }
    return false;
}
void App::showMessage(const InboxMessage& message) {
    if (!inbox_->markRead(message.ordinal)) { notice("INBOX READ ERROR"); return; }
    state_.selectedOrdinal = message.ordinal;
    state_.inboxPosition = inbox_->position(message.ordinal);
    memcpy(state_.received, message.text, sizeof(state_.received));
    state_.mode = Mode::Reading;
    refreshInbox(); notice("");
}
void App::notice(const char* message) {
    snprintf(state_.notice, sizeof(state_.notice), "%s", message);
}
void App::commit() {
    const size_t length = strlen(state_.draft);
    if (length == MaxText) { notice("TEXT FULL"); return; }
    char letter = ' ';
    if (state_.sequence[0]) {
        letter = decode(state_.sequence);
        if (!letter) { notice("INVALID - HOLD DOT TO ERASE"); return; }
    } else if (!length || state_.draft[length - 1] == ' ') return;
    state_.draft[length] = letter;
    state_.draft[length + 1] = '\0';
    state_.sequence[0] = '\0';
    state_.progress = 0;
}
void App::eraseWord() {
    state_.sequence[0] = '\0';
    state_.progress = 0;
    size_t n = strlen(state_.draft);
    while (n && state_.draft[n - 1] == ' ') --n;
    while (n && state_.draft[n - 1] != ' ') --n;
    state_.draft[n] = '\0';
    notice("LAST WORD ERASED");
}
void App::eraseLetter() {
    if (state_.sequence[0]) {
        state_.sequence[0] = '\0';
        state_.progress = 0;
        notice("PENDING LETTER ERASED");
        return;
    }
    size_t n = strlen(state_.draft);
    if (n) state_.draft[n - 1] = '\0';
    notice("LAST LETTER ERASED");
}
void App::resolveTaps() {
    if (tapCount_ == 1) {
        notice("");
        commit();
    } else if (tapCount_ == 2) {
        if (!openInbox()) notice("INBOX EMPTY");
    } else if (tapCount_ == 3) {
        if (state_.draft[0] || state_.sequence[0]) notice("DRAFT NOT EMPTY");
        else if (state_.lastSent[0]) {
            memcpy(state_.draft, state_.lastSent, sizeof(state_.draft));
            state_.delivery = Delivery::Idle;
            notice("LAST MESSAGE RESTORED");
        } else notice("NO LAST MESSAGE");
    } else if (tapCount_ >= 4) {
        state_.dictionaryPage = 0;
        state_.mode = Mode::Dictionary;
        notice("");
    }
    tapCount_ = 0;
}
void App::setButtonProgress(uint8_t progress, bool held) {
    buttonProgress_ = progress;
    buttonHeld_ = held;
}
void App::setPeerSignalBars(uint8_t bars) {
    state_.peerSignalBars = state_.peerConnected ? (bars > 3 ? 3 : bars) : 0;
}
void App::forceDisplayIdle(uint32_t now) {
    state_.displayIdle = true;
    lastActivityAt_ = now;
    tapCount_ = 0;
}
void App::press(Button button, bool longPress, uint32_t now) {
    if (state_.displayIdle) {
        state_.displayIdle = false;
        lastActivityAt_ = now;
        tapCount_ = 0;
        return;
    }
    lastActivityAt_ = now;
    const bool confirm = button == Button::Both && !longPress;
    if (state_.mode == Mode::Dictionary) {
        if (button == Button::Both) state_.mode = Mode::Compose;
        else if (!longPress && button == Button::Dot && state_.dictionaryPage) --state_.dictionaryPage;
        else if (!longPress && button == Button::Dash &&
             static_cast<size_t>(state_.dictionaryPage + 1) * 8 < dictionaryCount()) ++state_.dictionaryPage;
        return;
    }
    if (state_.mode == Mode::Incoming) {
        if (confirm) {
            InboxMessage message;
            if (inbox_->oldestUnread(message)) showMessage(message);
            else { state_.mode = Mode::Compose; state_.selectedOrdinal = 0;
                   state_.inboxPosition = 0; refreshInbox(); }
        }
        return;
    }
    if (state_.mode == Mode::Reading) {
        if (button == Button::Dash && longPress) {
            if (!clearInboxArmed_) {
                clearInboxArmed_ = true;
                notice("HOLD DASH AGAIN");
                return;
            }
            clearInboxArmed_ = false;
            InboxMessage message;
            while (inbox_->next(0, message)) {
                if (inbox_->erase(message.ordinal)) continue;
                refreshInbox();
                if (inbox_->next(0, message)) showMessage(message);
                else {
                    state_.mode = Mode::Compose;
                    state_.received[0] = '\0'; state_.selectedOrdinal = 0;
                    state_.inboxPosition = 0;
                }
                notice("CLEAR FAILED");
                return;
            }
            refreshInbox(); state_.received[0] = '\0'; state_.selectedOrdinal = 0;
            state_.inboxPosition = 0; state_.mode = Mode::Compose;
            notice("INBOX CLEARED");
            return;
        }
        if (clearInboxArmed_) {
            clearInboxArmed_ = false;
            notice("");
        }
        if (button == Button::Dot && longPress) {
            if (!inbox_->erase(state_.selectedOrdinal)) { notice("DELETE FAILED"); return; }
            refreshInbox(); state_.received[0] = '\0'; state_.selectedOrdinal = 0;
            state_.inboxPosition = 0; state_.mode = Mode::Compose;
            notice("MESSAGE DELETED"); return;
        }
        if (confirm) { state_.mode = Mode::Compose; state_.received[0] = '\0';
                       state_.selectedOrdinal = 0; state_.inboxPosition = 0; notice(""); return; }
        InboxMessage message;
        const bool found = !longPress && button == Button::Dot ?
            inbox_->previous(state_.selectedOrdinal, message) :
            !longPress && button == Button::Dash && inbox_->next(state_.selectedOrdinal, message);
        if (found) showMessage(message);
        return;
    }
    if (state_.delivery == Delivery::Sending) { notice("WAITING FOR ACK"); return; }
    if (button != Button::Both) { if (tapCount_) resolveTaps(); notice(""); }
    if (button == Button::Both && longPress) {
        tapCount_ = 0;
        sendMessage(now); return;
    }
    if (button == Button::Both && !longPress) {
        ++tapCount_;
        tapAt_ = now;
        return;
    }
    if (confirm) {
        commit();
        return;
    }
    if (longPress) {
        if (button == Button::Dot) eraseWord();
        if (button == Button::Dash) eraseLetter();
        return;
    }
    const size_t n = strlen(state_.sequence);
    if (n == MaxSequence) { notice("SYMBOL FULL - HOLD DOT TO ERASE"); return; }
    state_.sequence[n] = button == Button::Dot ? '.' : '-';
    state_.sequence[n + 1] = '\0';
    symbolAt_ = now;
}
void App::sendMessage(uint32_t now) {
    if (state_.sequence[0]) {
        commit();
        if (state_.sequence[0]) return;
    }
    size_t n = strlen(state_.draft);
    while (n && state_.draft[n - 1] == ' ') state_.draft[--n] = '\0';
    if (!n) { notice("NOTHING TO SEND"); return; }
    // Manual retry of an unchanged failed draft retains its identity.
    if (!(state_.delivery == Delivery::Failed && strcmp(pending_.text, state_.draft) == 0)) {
        pending_ = Packet{};
        pending_.session = session_;
        pending_.id = nextId_++;
        if (!nextId_) { ++session_; if (!session_) ++session_; nextId_ = 1; }
        memcpy(pending_.text, state_.draft, n + 1);
    }
    memcpy(state_.lastSent, state_.draft, n + 1);
    state_.messageId = pending_.id;
    state_.attempts = 0;
    state_.delivery = Delivery::Sending;
    attempt(now);
}
void App::attempt(uint32_t now) {
    uint8_t bytes[MaxPacket];
    const size_t length = serialize(pending_, bytes, sizeof(bytes));
    ++state_.attempts;
    sentAt_ = now;
    if (!transport_.send(bytes, length)) notice("TRANSPORT BUSY / NOT READY");
    else notice("");
}
void App::tick(uint32_t now) {
    state_.cursorVisible = ((now / 500) & 1u) == 0;
    if (!state_.displayIdle && uint32_t(now - lastActivityAt_) >= DisplayIdleMs) {
        state_.displayIdle = true;
        tapCount_ = 0;
    }
    const uint32_t tapDelay = tapCount_ >= 4 ? DictionaryTapDelayMs : MultiTapMs;
    if (tapCount_ && uint32_t(now - tapAt_) >= tapDelay) resolveTaps();
    if (state_.mode == Mode::Compose && state_.sequence[0] && !buttonHeld_ &&
        uint32_t(now - symbolAt_) >= AutoConfirmMs) {
        commit();
        if (state_.sequence[0]) symbolAt_ = now;
    }
    if (buttonHeld_) state_.progress = buttonProgress_;
    else if (state_.mode == Mode::Compose && state_.sequence[0]) {
        const uint32_t elapsed = now - symbolAt_;
        state_.progress = static_cast<uint8_t>(elapsed >= AutoConfirmMs ? 100 : elapsed * 100 / AutoConfirmMs);
    } else state_.progress = 0;
    if (state_.peerConnected && uint32_t(now - peerAt_) >= 6000) {
        state_.peerConnected = false;
        state_.peerSignalBars = 0;
        unlinkedAt_ = now;
        unlinkTimerStarted_ = true;
    }
    if (peerEverConnected_ && unlinkTimerStarted_ && !state_.deepSleepRequested &&
        uint32_t(now - unlinkedAt_) >= DeepSleepAfterUnlinkMs) {
        state_.deepSleepRequested = true;
    }
    if (!pingStarted_ || uint32_t(now - pingAt_) >= 2000) {
        pingStarted_ = true; pingAt_ = now;
        if (++pingId_ == 0) ++pingId_;
        Packet ping; ping.type = PacketType::Ping; ping.session = session_; ping.id = pingId_;
        uint8_t bytes[MaxPacket];
        const auto length = serialize(ping, bytes, sizeof(bytes));
        transport_.send(bytes, length);
    }
    if (state_.delivery != Delivery::Sending || uint32_t(now - sentAt_) < AckTimeoutMs) return;
    if (state_.attempts < MaxAttempts) attempt(now);
    else {
        state_.delivery = Delivery::Failed;
        notice("NO ACK - HOLD BOTH TO RETRY");
    }
}
void App::receive(const uint8_t* bytes, size_t length, uint32_t now) {
    Packet packet;
    if (!deserialize(bytes, length, packet)) return;
    if (packet.type == PacketType::Ping) {
        packet.type = PacketType::Pong;
        uint8_t response[MaxPacket];
        const auto size = serialize(packet, response, sizeof(response));
        transport_.send(response, size);
        return;
    }
    if (packet.type == PacketType::Pong) {
        if (pingStarted_ && packet.session == session_ && packet.id == pingId_ &&
            uint32_t(now - pingAt_) < 2000) {
            peerAt_ = now; state_.peerConnected = true;
            peerEverConnected_ = true;
            unlinkTimerStarted_ = false;
            state_.deepSleepRequested = false;
        }
        return;
    }
    if (packet.type == PacketType::Ack) {
        if ((state_.delivery == Delivery::Sending || state_.delivery == Delivery::Failed) &&
            packet.session == pending_.session && packet.id == pending_.id) {
            state_.delivery = Delivery::Delivered;
            // A late ACK must not erase a draft edited after a failure.
            if (strcmp(state_.draft, pending_.text) == 0) state_.draft[0] = '\0';
            notice("");
        }
        return;
    }
    state_.displayIdle = false;
    lastActivityAt_ = now;
    const auto result = inbox_->append(packet.session, packet.id, packet.text);
    if (result == StoreResult::Full) { notice("INBOX FULL - NO ACK"); return; }
    if (result == StoreResult::Error) { notice("INBOX STORAGE ERROR"); return; }
    if (result == StoreResult::Stored) {
        ++state_.receivedCount;
        refreshInbox();
    } else ++state_.duplicates;
    // Always ACK repeats: the original ACK may have been lost.
    packet.type = PacketType::Ack; packet.text[0] = '\0';
    uint8_t ack[MaxPacket];
    const size_t ackLength = serialize(packet, ack, sizeof(ack));
    transport_.send(ack, ackLength);
}
void InputManager::sample(bool dot, bool dash, uint32_t now, App& app) {
    if (app.state().mode == Mode::Dictionary) displayIdleTapCount_ = 0;
    const bool raw[] = {dot, dash};
    bool released[2] = {};
    for (unsigned i = 0; i < 2; ++i) {
        auto& key = keys_[i];
        if (raw[i] != key.raw) { key.raw = raw[i]; key.changed = now; }
        if (key.raw != key.stable && uint32_t(now - key.changed) >= DebounceMs) {
            key.stable = key.raw;
            if (key.stable) { key.pressedAt = now; key.longFired = false; }
            else released[i] = true;
        }
    }
    if (!chord_ && keys_[0].stable && keys_[1].stable &&
        !keys_[0].longFired && !keys_[1].longFired) {
        chord_ = true; chordLong_ = false; chordAt_ = now;
    }
    if (chord_) {
        if (keys_[0].stable && keys_[1].stable && dot && dash && !chordLong_ &&
            uint32_t(now - chordAt_) >= LongPressMs) {
            chordLong_ = true; app.press(Button::Both, true, now);
        }
        // Consume both releases, even if the user releases one much earlier.
        if (!keys_[0].stable && !keys_[1].stable) {
            if (!chordLong_) {
                const bool waking = app.state().displayIdle;
                app.press(Button::Both, false, now);
                if (waking) {
                    displayIdleTapCount_ = 0;
                } else {
                    if (displayIdleTapCount_ && uint32_t(now - lastDisplayIdleTapAt_) > ForcedDisplayIdleSequenceGapMs)
                        displayIdleTapCount_ = 0;
                    lastDisplayIdleTapAt_ = now;
                    if (++displayIdleTapCount_ >= ForcedDisplayIdleTaps) {
                        displayIdleTapCount_ = 0;
                        app.forceDisplayIdle(now);
                    }
                }
            } else {
                displayIdleTapCount_ = 0;
            }
            chord_ = false;
        }
    } else {
        if (released[0] || released[1]) displayIdleTapCount_ = 0;
        for (unsigned i = 0; i < 2; ++i) {
            auto& key = keys_[i];
            if (released[i] && !key.longFired) app.press(static_cast<Button>(i), false, now);
            if (key.stable && key.raw && !key.longFired &&
                uint32_t(now - key.pressedAt) >= LongPressMs) {
                key.longFired = true; app.press(static_cast<Button>(i), true, now);
            }
        }
    }
    uint32_t elapsed = 0;
    bool held = false;
    if (chord_ && (keys_[0].stable || keys_[1].stable)) {
        held = true; elapsed = now - chordAt_;
    } else for (const auto& current : keys_) if (current.stable) {
        held = true;
        if (now - current.pressedAt > elapsed) elapsed = now - current.pressedAt;
    }
    app.setButtonProgress(static_cast<uint8_t>(elapsed >= LongPressMs ? 100 : elapsed * 100 / LongPressMs), held);
}
void renderLandscape(const State& state, const char* name, IScreen& screen) {
    constexpr uint16_t white = 0xef7b, dim = 0x8c71, amber = 0xfdaa, mint = 0x7f36;
    screen.clear(0x0842);
    if (state.displayIdle) return;
    char line[40];
    char identity[2] = {name && (name[0] == 'A' || name[0] == 'B') ? name[0] : '?', '\0'};
    screen.text(6, 5, identity, 1, amber);
    screen.text(18, 5, state.peerConnected ? "LINK" : "UNLINK", 1, state.peerConnected ? mint : dim);
    screen.text(110, 5, deliveryName(state.delivery), 1,
                state.delivery == Delivery::Failed ? 0xfa69 : mint);
    screen.signalBars(60, 5, state.peerConnected ? state.peerSignalBars : 0, mint);
    snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(state.inboxRemaining));
    screen.text(110, 14, line, 1, dim);
    screen.dial(228, 10, state.progress, amber);
    if (state.mode == Mode::Incoming) {
        screen.text(6, 35, "NEW MESSAGE", 2, amber);
        snprintf(line, sizeof(line), "%lu unread  %lu stored",
            static_cast<unsigned long>(state.unreadCount), static_cast<unsigned long>(state.inboxCount));
        screen.text(6, 57, line, 1, mint);
        screen.text(6, 68, "BOTH: accept and read", 1, white);
        screen.text(6, 105, "Message hidden until accepted", 1, dim);
        return;
    }
    if (state.mode == Mode::Reading) {
        screen.text(6, 30, "RECEIVED", 1, mint);
        snprintf(line, sizeof(line), "MSG %lu/%lu", static_cast<unsigned long>(state.inboxPosition),
                 static_cast<unsigned long>(state.inboxCount));
        screen.text(90, 30, line, 1, amber);
        twoLines(screen, state.received, 50, white);
        screen.text(6, 100, "DOT/DASH browse  BOTH compose", 1, amber);
        screen.text(6, 115, "Hold DOT: delete message", 1, dim);
        if (state.notice[0]) screen.text(6, 123, state.notice, 1, amber);
        else screen.text(6, 125, "hold DASH x2: clear", 1, dim);
        return;
    }
    if (state.mode == Mode::Dictionary) {
        snprintf(line, sizeof(line), "MORSE %u/%u", state.dictionaryPage + 1,
                 static_cast<unsigned>((dictionaryCount() + 7) / 8));
        screen.text(6, 20, line, 1, amber);
        for (unsigned row = 0; row < 4; ++row) {
            for (unsigned column = 0; column < 2; ++column) {
                const size_t index = state.dictionaryPage * 8 + column * 4 + row;
                if (index >= dictionaryCount()) continue;
                snprintf(line, sizeof(line), "%c %s", dictionarySymbol(index), dictionaryCode(index));
                screen.text(6 + column * 118, 34 + row * 13, line, 1, white);
            }
        }
        screen.text(6, 118, "DOT <  DASH >  BOTH close", 1, dim);
        return;
    }
    screen.text(6, 22, state.sequence[0] ? state.sequence : "_", 2, amber);
    if (state.sequence[0]) {
        const char preview = decode(state.sequence);
        char hint[] = {'=', ' ', preview ? preview : '?', '\0'};
        screen.text(93, 27, hint, 1, mint);
    }
    const bool showSent = !state.draft[0] && state.lastSent[0] && !state.sequence[0];
    snprintf(line, sizeof(line), "%s %luN %luS", showSent ? "LAST SENT" : "COMPOSING",
        static_cast<unsigned long>(state.unreadCount), static_cast<unsigned long>(state.inboxCount));
    screen.text(6, 45, line, 1, dim);
    if (state.unreadCount) screen.text(190, 45, "NEW MSG", 1, amber);
    if (showSent) twoLines(screen, state.lastSent, 56, white);
    else if (state.draft[0]) twoLines(screen, state.draft, 56, white);
    if (!showSent && state.cursorVisible) {
        const size_t length = strlen(state.draft);
        if (length < 38) screen.text(6 + int(length) * 6, 56, "_", 1, amber);
        else screen.text(6 + int(length - 38) * 6, 66, "_", 1, amber);
    }
    if (state.notice[0]) screen.text(6, 123, state.notice, 1, amber);
}
void renderPortrait(const State& state, const char* name, IScreen& screen) {
    constexpr uint16_t white = 0xef7b, dim = 0x8c71, amber = 0xfdaa, mint = 0x7f36;
    screen.clear(0x0842);
    if (state.displayIdle) return;
    char line[40];
    char identity[2] = {name && (name[0] == 'A' || name[0] == 'B') ? name[0] : '?', '\0'};
    screen.text(5, 5, identity, 1, amber);
    screen.text(17, 5, state.peerConnected ? "LINK" : "UNLINK", 1,
                state.peerConnected ? mint : dim);
    screen.dial(123, 11, state.progress, amber);
    screen.text(5, 21, deliveryName(state.delivery), 1,
                state.delivery == Delivery::Failed ? 0xfa69 : mint);
    screen.signalBars(59, 5, state.peerConnected ? state.peerSignalBars : 0, mint);
    snprintf(line, sizeof(line), "%lu", static_cast<unsigned long>(state.inboxRemaining));
    screen.text(5, 33, line, 1, dim);
    snprintf(line, sizeof(line), "%luN %luS",
        static_cast<unsigned long>(state.unreadCount), static_cast<unsigned long>(state.inboxCount));
    screen.text(5, 45, line, 1, dim);
    if (state.unreadCount) screen.text(88, 45, "NEW MSG", 1, amber);
    if (state.mode == Mode::Dictionary) {
        snprintf(line, sizeof(line), "MORSE %u/%u", state.dictionaryPage + 1,
                 static_cast<unsigned>((dictionaryCount() + 7) / 8));
        screen.text(5, 59, line, 1, amber);
        for (unsigned row = 0; row < 8; ++row) {
            const size_t index = state.dictionaryPage * 8 + row;
            if (index >= dictionaryCount()) break;
            snprintf(line, sizeof(line), "%c  %s", dictionarySymbol(index), dictionaryCode(index));
            screen.text(8, 75 + row * 17, line, 1, white);
        }
        screen.text(5, 219, "DOT< DASH> BOTH exit", 1, dim);
        return;
    }
    if (state.mode == Mode::Incoming) {
        screen.text(5, 66, "NEW MESSAGE", 1, amber);
        screen.text(5, 93, "BOTH to open", 1, white);
        screen.text(5, 209, "Contents hidden", 1, dim);
        return;
    }
    auto wrapped = [&screen](const char* value, int y, uint16_t color) {
        const size_t length = strlen(value);
        for (size_t start = 0; start < length && start < MaxText; start += 20) {
            char part[21] = {};
            const size_t n = std::min<size_t>(20, length - start);
            memcpy(part, value + start, n);
            screen.text(5, y + int(start / 20) * 13, part, 1, color);
        }
        if (!length) screen.text(5, y, "(empty)", 1, dim);
    };
    if (state.mode == Mode::Reading) {
        screen.text(5, 59, "RECEIVED", 1, mint);
        snprintf(line, sizeof(line), "MSG %lu/%lu", static_cast<unsigned long>(state.inboxPosition),
                 static_cast<unsigned long>(state.inboxCount));
        screen.text(5, 71, line, 1, amber);
        wrapped(state.received, 89, white);
        screen.text(5, 185, "DOT/DASH browse", 1, dim);
        screen.text(5, 198, "BOTH compose", 1, amber);
        screen.text(5, 211, "hold DOT delete", 1, dim);
        if (state.notice[0]) {
            char brief[21] = {};
            strncpy(brief, state.notice, 20);
            screen.text(5, 223, brief, 1, amber);
        } else screen.text(5, 223, "hold DASH x2: clear", 1, dim);
        return;
    }
    screen.text(5, 59, state.sequence[0] ? state.sequence : "_", 2, amber);
    if (state.sequence[0]) {
        const char preview = decode(state.sequence);
        char hint[] = {'=', ' ', preview ? preview : '?', '\0'};
        screen.text(5, 82, hint, 1, mint);
    }
    const bool showSent = !state.draft[0] && state.lastSent[0] && !state.sequence[0];
    screen.text(5, 97, showSent ? "LAST SENT" : "DRAFT", 1, dim);
    if (showSent) wrapped(state.lastSent, 113, white);
    else if (state.draft[0]) wrapped(state.draft, 113, white);
    if (!showSent && state.cursorVisible) {
        const size_t length = strlen(state.draft);
        screen.text(5 + int(length % 20) * 6, 113 + int(length / 20) * 13, "_", 1, amber);
    }
    if (state.notice[0]) {
        char brief[21] = {};
        strncpy(brief, state.notice, 20);
        screen.text(5, 223, brief, 1, amber);
    }
}
void render(const State& state, const char* name, IScreen& screen, ScreenLayout layout) {
    if (layout == ScreenLayout::Landscape) renderLandscape(state, name, screen);
    else renderPortrait(state, name, screen);
}
}
