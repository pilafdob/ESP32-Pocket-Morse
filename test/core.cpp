#include <MorseCore.h>
#include <SecureChannel.h>
#include <SimPair.h>
#include <InboxJournal.h>
#include <monocypher.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <vector>
#include <utility>
#include <algorithm>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#include "../lib/Monocypher/tests/vectors.h"
#pragma GCC diagnostic pop
using namespace morse;
struct Capture : ITransport {
    std::vector<std::vector<uint8_t>> sent;
    bool available = true;
    bool send(const uint8_t* bytes, size_t length) override {
        sent.emplace_back(bytes, bytes + length); return available;
    }
    size_t texts() const {
        size_t n = 0; for (auto& b : sent) { Packet p; if (deserialize(b.data(), b.size(), p) && p.type == PacketType::Text) ++n; } return n;
    }
};
void compose(App& app, const char* text, uint32_t now = 1) {
    for (const char* p = text; *p; ++p) {
        if (*p != ' ') for (const char* c = encode(*p); *c; ++c)
            app.press(*c == '.' ? Button::Dot : Button::Dash, false, now);
        app.press(Button::Both, false, now);
        app.tick(now + MultiTapMs);
    }
}
void inject(App& app, Packet p, uint32_t now) {
    uint8_t bytes[MaxPacket]; const auto n = serialize(p, bytes, sizeof(bytes)); assert(n);
    app.receive(bytes, n, now);
}
std::vector<uint8_t> unhex(const char* s) {
    std::vector<uint8_t> out(strlen(s) / 2);
    for (size_t i = 0; i < out.size(); ++i) { unsigned n; assert(sscanf(s + i * 2, "%2x", &n) == 1); out[i] = n; }
    return out;
}
void vectors() {
    for (size_t i = 0; i < nb_aead_ietf_vectors; i += 5) {
        auto key = unhex(aead_ietf_vectors[i]); auto nonce = unhex(aead_ietf_vectors[i + 1]);
        auto ad = unhex(aead_ietf_vectors[i + 2]); auto plain = unhex(aead_ietf_vectors[i + 3]);
        auto expected = unhex(aead_ietf_vectors[i + 4]); std::vector<uint8_t> out(expected.size());
        crypto_aead_lock(out.data() + 16, out.data(), key.data(), nonce.data(), ad.data(), ad.size(), plain.data(), plain.size());
        assert(out == expected);
        std::vector<uint8_t> decoded(plain.size() + 1);
        assert(crypto_aead_unlock(decoded.data(), out.data(), key.data(), nonce.data(), ad.data(), ad.size(), out.data() + 16, plain.size()) == 0);
        assert(plain.empty() || memcmp(decoded.data(), plain.data(), plain.size()) == 0);
    }
    printf("PASS %zu upstream XChaCha20-Poly1305 known-answer vectors\n", nb_aead_ietf_vectors / 5);
}
void codecs() {
    const char* alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    const char* codes[] = {".-","-...","-.-.","-..",".","..-.","--.","....","..",".---","-.-",".-..","--","-.","---",".--.","--.-",".-.","...","-","..-","...-",".--","-..-","-.--","--..","-----",".----","..---","...--","....-",".....","-....","--...","---..","----."};
    for (unsigned i = 0; i < 36; ++i) { assert(decode(codes[i]) == alphabet[i]); assert(strcmp(encode(alphabet[i]), codes[i]) == 0); }
    assert(dictionaryCount() == 49);
    for (size_t i = 0; i < dictionaryCount(); ++i)
        assert(decode(dictionaryCode(i)) == dictionarySymbol(i));
    assert(!decode("...---...") && !decode("") && !decode(".-.-") && !decode(nullptr));
    assert(strcmp(encode('?'), "..--..") == 0 && !encode('\0'));
    Packet p; p.session = 0x12345678; p.id = 0xaabbccdd; strcpy(p.text, "HELLO 42");
    uint8_t bytes[MaxPacket]; const auto n = serialize(p, bytes, sizeof(bytes)); assert(n == 25);
    assert(bytes[4] == 0x78 && bytes[7] == 0x12 && bytes[12] == 0xdd && bytes[17] == 16 && bytes[18] == 2);
    Packet copy; assert(deserialize(bytes, n, copy) && strcmp(copy.text, p.text) == 0);
    strcpy(p.text, "HI, YOU? \"YES\".");
    const auto punctuationLength = serialize(p, bytes, sizeof(bytes));
    assert(deserialize(bytes, punctuationLength, copy) && strcmp(copy.text, p.text) == 0);
    strcpy(p.text, "HELLO 42"); serialize(p, bytes, sizeof(bytes));
    for (size_t size = 0; size < n; ++size) assert(!deserialize(bytes, size, copy));
    bytes[2] = 99; assert(!deserialize(bytes, n, copy)); bytes[2] = 2;
    bytes[3] = 99; assert(!deserialize(bytes, n, copy)); bytes[3] = 1;
    bytes[17] = 1; assert(!deserialize(bytes, n, copy)); bytes[17] = 255; assert(!deserialize(bytes, n, copy));
    memset(p.text, 'Z', MaxText); p.text[MaxText] = 0;
    assert(serialize(p, bytes, sizeof(bytes)) == MaxPacket && deserialize(bytes, MaxPacket, copy));
    assert(!serialize(p, bytes, MaxPacket - 1));
    uint32_t random = 17;
    for (unsigned i = 0; i < 10000; ++i) {
        for (auto& b : bytes) { random = random * 1664525u + 1013904223u; b = random >> 24; }
        deserialize(bytes, i % (MaxPacket + 1), copy);
    }
    puts("PASS Morse mappings, binary-Morse wire format, malformed packets and 10000 parser probes");
}
void usability() {
    Capture transport; App app(transport, 77);
    app.press(Button::Dot, false, 0); app.tick(250);
    assert(strcmp(app.state().sequence, ".") == 0 && app.state().progress == 50 && app.state().cursorVisible);
    app.tick(499); assert(!app.state().draft[0] && app.state().sequence[0] == '.');
    app.tick(500); assert(strcmp(app.state().draft, "E") == 0 && !app.state().sequence[0] && !app.state().cursorVisible);
    app.press(Button::Dash, false, 1100); app.press(Button::Both, false, 1200);
    app.tick(1700); assert(strcmp(app.state().draft, "ET") == 0);
    app.press(Button::Dash, true, 1800); assert(strcmp(app.state().draft, "E") == 0);
    app.press(Button::Both, false, 1900); app.tick(2400);
    assert(strcmp(app.state().draft, "E ") == 0);
    app.press(Button::Dot, false, 2500); app.press(Button::Dot, true, 2600);
    assert(!app.state().draft[0] && !app.state().sequence[0]);
    app.press(Button::Dot, false, 2700); app.tick(3700);
    app.press(Button::Both, false, 3800); app.tick(4300);
    app.press(Button::Dash, false, 4400); app.tick(5400);
    assert(strcmp(app.state().draft, "E T") == 0);
    app.press(Button::Dot, true, 5500); assert(strcmp(app.state().draft, "E ") == 0);
    app.press(Button::Dash, true, 5550); assert(strcmp(app.state().draft, "E") == 0);
    app.press(Button::Both, true, 5700);
    assert(app.state().delivery == Delivery::Sending && strcmp(app.state().lastSent, "E") == 0);
    Packet ack; ack.type = PacketType::Ack; ack.session = 77; ack.id = 1; inject(app, ack, 5800);
    assert(app.state().delivery == Delivery::Delivered && !app.state().draft[0]);
    app.press(Button::Both, false, 5900); app.press(Button::Both, false, 6000);
    app.press(Button::Both, false, 6100); app.tick(6600);
    assert(strcmp(app.state().draft, "E") == 0);
    app.press(Button::Both, false, 6700); app.press(Button::Both, false, 6800);
    app.press(Button::Both, false, 6900); app.press(Button::Both, false, 7000);
    assert(app.state().mode == Mode::Dictionary);
    app.press(Button::Dash, false, 7000); assert(app.state().dictionaryPage == 1);
    app.press(Button::Dot, false, 7100); assert(app.state().dictionaryPage == 0);
    app.press(Button::Both, false, 7200); assert(app.state().mode == Mode::Compose);
    puts("PASS 500-ms auto-confirm, timer, letter/word erase, both gestures, recall and dictionary");
}
struct MockFlash : IByteStore, IEntropy {
    std::vector<uint8_t> data;
    uint32_t limit = 2 * InboxRecordSize;
    uint8_t sequence = 1;
    bool failWrites = false;
    uint32_t size() const override { return static_cast<uint32_t>(data.size()); }
    uint32_t capacity() const override { return limit; }
    bool read(uint32_t offset, uint8_t* out, size_t length) override {
        if (offset > data.size() || length > data.size() - offset) return false;
        memcpy(out, data.data() + offset, length); return true;
    }
    bool write(uint32_t offset, const uint8_t* bytes, size_t length) override {
        if (failWrites || offset > data.size() ||
            (offset + length > data.size() && offset + length > limit)) return false;
        if (offset + length > data.size()) data.resize(offset + length);
        memcpy(data.data() + offset, bytes, length); return true;
    }
    bool fill(uint8_t* bytes, size_t length) override {
        for (size_t i = 0; i < length; ++i) bytes[i] = sequence++;
        return true;
    }
};
void journalTests() {
    uint8_t key[32]; for (unsigned i=0;i<32;++i) key[i]=i+41;
    MockFlash flash;
    {
        InboxJournal journal(flash,flash); assert(journal.begin(key,0));
        assert(journal.remaining() == 2);
        assert(journal.append(4,1,"FIRST SECRET") == StoreResult::Stored);
        assert(journal.remaining() == 1);
        assert(journal.append(4,1,"FIRST SECRET") == StoreResult::Duplicate);
        assert(journal.append(4,2,"SECOND") == StoreResult::Stored);
        assert(journal.count() == 2 && journal.unreadCount() == 2);
        assert(journal.remaining() == 0);
        assert(journal.append(4,3,"THIRD") == StoreResult::Full);
        InboxMessage message; assert(journal.oldestUnread(message) && strcmp(message.text,"FIRST SECRET") == 0);
        assert(journal.markRead(message.ordinal) && journal.unreadCount() == 1);
        assert(journal.next(message.ordinal,message) && strcmp(message.text,"SECOND") == 0);
        assert(journal.erase(message.ordinal) && journal.count() == 1);
        assert(journal.remaining() == 1 && journal.position(1) == 1);
        assert(journal.append(4,3,"THIRD") == StoreResult::Stored && journal.count() == 2);
        assert(journal.remaining() == 0 && journal.position(3) == 2);
    }
    const char* raw = reinterpret_cast<const char*>(flash.data.data());
    const char secret[] = "FIRST SECRET";
    assert(std::search(raw,raw+flash.data.size(),secret,secret+strlen(secret)) == raw+flash.data.size());
    InboxJournal reboot(flash,flash); assert(reboot.begin(key,0));
    assert(reboot.count() == 2 && reboot.unreadCount() == 1);
    assert(reboot.remaining() == 0 && reboot.position(3) == 2);
    assert(reboot.append(4,1,"FIRST SECRET") == StoreResult::Duplicate);
    InboxMessage message; assert(reboot.oldestUnread(message) && strcmp(message.text,"THIRD") == 0);
    MockFlash legacyFlash = flash; legacyFlash.limit = InboxRecordSize;
    InboxJournal legacy(legacyFlash, legacyFlash); assert(legacy.begin(key,0));
    assert(legacy.remaining() == 0 && legacy.count() == 2);
    assert(legacy.erase(message.ordinal) && legacy.remaining() == 1);
    assert(legacy.append(4,4,"REUSED") == StoreResult::Stored && legacy.remaining() == 0);
    key[0]^=1; InboxJournal wrong(flash,flash); assert(!wrong.begin(key,0)); key[0]^=1;
    flash.data[70]^=1; InboxJournal corrupt(flash,flash); assert(!corrupt.begin(key,0)); flash.data[70]^=1;
    MockFlash one; one.limit = InboxRecordSize;
    InboxJournal durable(one,one); assert(durable.begin(key,1));
    Capture transport; App app(transport,55); app.attachInbox(durable);
    Packet incoming; incoming.session=66; incoming.id=1; strcpy(incoming.text,"ONE"); inject(app,incoming,1);
    assert(app.state().inboxCount == 1 && transport.sent.size() == 1);
    ++incoming.id; strcpy(incoming.text,"TWO"); inject(app,incoming,2);
    assert(app.state().inboxCount == 1 && transport.sent.size() == 1);
    puts("PASS encrypted flash journal reboot, fill, deletion/reuse, duplicates, corruption and ACK-after-store");
}
struct BoundsScreen : IScreen {
    int width, height;
    std::vector<std::pair<int, int>> underscores;
    explicit BoundsScreen(int w, int h) : width(w), height(h) {}
    void clear(uint16_t) override {}
    void text(int x, int y, const char* value, int size, uint16_t) override {
        assert(x >= 0 && y >= 0 && x + int(strlen(value)) * 6 * size <= width);
        assert(y + 8 * size <= height);
        if (strcmp(value, "_") == 0) underscores.emplace_back(x, y);
    }
    void dial(int x, int y, uint8_t progress, uint16_t) override {
        assert(x >= 8 && x + 8 < width && y >= 8 && y + 8 < height && progress <= 100);
    }
};
void screenLayouts() {
    State state;
    state.inboxCount = 101234; state.unreadCount = 101234;
    state.inboxPosition = 101234; state.inboxRemaining = 101234;
    strcpy(state.draft,"ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789.,:?'-/()\"=+@");
    for (const auto mode : {Mode::Compose,Mode::Incoming,Mode::Reading,Mode::Dictionary}) {
        state.mode = mode;
        for (unsigned page=0;page<7;++page) {
            state.dictionaryPage = page;
            BoundsScreen portrait(135,240), landscape(240,135);
            render(state,"A",portrait,ScreenLayout::Portrait);
            render(state,"A",landscape,ScreenLayout::Landscape);
        }
    }
    state.mode = Mode::Compose; state.cursorVisible = true;
    BoundsScreen portrait(135,240), landscape(240,135);
    render(state,"A",portrait,ScreenLayout::Portrait);
    render(state,"A",landscape,ScreenLayout::Landscape);
    assert(std::find(portrait.underscores.begin(), portrait.underscores.end(),
        std::make_pair(5 + int(strlen(state.draft) % 20) * 6, 113 + int(strlen(state.draft) / 20) * 13)) != portrait.underscores.end());
    assert(std::find(landscape.underscores.begin(), landscape.underscores.end(),
        std::make_pair(strlen(state.draft) < 38 ? 6 + int(strlen(state.draft)) * 6 : 6 + int(strlen(state.draft) - 38) * 6,
                       strlen(state.draft) < 38 ? 56 : 66)) != landscape.underscores.end());
    puts("PASS portrait and preserved landscape bounds across all screen modes/dictionary pages");
}
void inputs() {
    Capture t; App app(t, 10); InputManager keys;
    keys.sample(true,false,0,app); keys.sample(false,false,5,app);
    keys.sample(true,false,10,app); keys.sample(true,false,40,app);
    keys.sample(false,false,60,app); keys.sample(true,false,65,app);
    keys.sample(false,false,70,app); keys.sample(false,false,100,app);
    assert(strcmp(app.state().sequence, ".") == 0);
    app.tick(1100); assert(strcmp(app.state().draft, "E") == 0);
    keys.sample(true,true,1200,app); keys.sample(true,true,1230,app);
    keys.sample(true,true,2030,app); keys.sample(true,true,2200,app);
    keys.sample(false,true,2210,app); keys.sample(false,true,2240,app);
    keys.sample(false,false,2300,app); keys.sample(false,false,2330,app);
    assert(t.texts() == 1 && strcmp(app.state().draft,"E") == 0 && !app.state().sequence[0]);
    puts("PASS two-button bouncing, auto-confirm, chord send and staggered release suppression");
}
void compositionAndInbox() {
    Capture t; App a(t, 1); compose(a, "SOS 42"); assert(strcmp(a.state().draft, "SOS 42") == 0);
    a.press(Button::Dot,true,2); assert(strcmp(a.state().draft,"SOS ") == 0);
    Packet incoming; incoming.session=2; incoming.id=1; strcpy(incoming.text,"HELLO");
        inject(a,incoming,3); assert(a.state().mode == Mode::Compose && !a.state().received[0]);
        assert(a.state().inboxRemaining == 1023 && a.state().inboxCount == 1 &&
            a.state().unreadCount == 1 && strcmp(a.state().draft,"SOS ") == 0);
        a.press(Button::Both,false,4); a.press(Button::Both,false,5); a.tick(5 + MultiTapMs);
        assert(a.state().mode == Mode::Reading && strcmp(a.state().received,"HELLO") == 0 &&
            strcmp(a.state().draft,"SOS ") == 0);
    const auto sent=t.sent.size(); ++incoming.id; inject(a,incoming,6);
    assert(t.sent.size() == sent + 1 && a.state().receivedCount == 2 && a.state().unreadCount == 1);
    a.press(Button::Dash,false,7); assert(strcmp(a.state().received,"HELLO") == 0 && a.state().unreadCount == 0 && a.state().inboxPosition == 2);
    a.press(Button::Dot,false,8); assert(a.state().selectedOrdinal == 1);
    a.press(Button::Both,false,7); assert(a.state().mode == Mode::Compose && strcmp(a.state().draft,"SOS ") == 0);
    inject(a,incoming,8); assert(a.state().mode == Mode::Compose && a.state().receivedCount == 2);
    inject(a,incoming,9); assert(a.state().receivedCount == 2 && a.state().duplicates == 2);
    puts("PASS multi-message inbox, browsing, preserved draft and duplicate receipts");
}
void deletionFeatures() {
    Capture transport; MemoryInbox inbox; App app(transport, 99); app.attachInbox(inbox);
    assert(inbox.append(1, 1, "FIRST") == StoreResult::Stored);
    assert(inbox.append(1, 2, "SECOND") == StoreResult::Stored);
    app.attachInbox(inbox);
    assert(app.state().mode == Mode::Compose && app.state().inboxCount == 2 && app.state().unreadCount == 2);
    app.press(Button::Both, false, 1); app.press(Button::Both, false, 2); app.tick(2 + MultiTapMs);
    assert(app.state().mode == Mode::Reading);
    app.press(Button::Dash, true, 2);
    assert(app.state().inboxCount == 2 && strcmp(app.state().notice, "HOLD DASH AGAIN") == 0);
    app.press(Button::Dot, false, 3);
    assert(app.state().inboxCount == 2 && !app.state().notice[0]);
    app.press(Button::Dash, true, 4);
    app.press(Button::Dash, true, 5);
    assert(app.state().mode == Mode::Compose && app.state().inboxCount == 0 &&
           strcmp(app.state().notice, "INBOX CLEARED") == 0);
    puts("PASS two-hold confirmation before clearing the entire inbox");
}
void reliability() {
    Capture ta,tb; App a(ta,1),b(tb,2); compose(a,"HELLO"); a.press(Button::Both,true,10);
    const auto original=*std::find_if(ta.sent.begin(), ta.sent.end(), [](const auto& bytes) {
        Packet packet; return deserialize(bytes.data(), bytes.size(), packet) && packet.type == PacketType::Text;
    }); a.tick(709); assert(ta.texts() == 1);
    a.tick(710); assert(ta.texts() == 2 && a.state().attempts == 2);
    b.receive(original.data(),original.size(),720); b.receive(original.data(),original.size(),730);
    assert(b.state().receivedCount == 1 && b.state().duplicates == 1);
    Packet ack; assert(deserialize(original.data(),original.size(),ack)); ack.type=PacketType::Ack; ack.text[0]=0;
    ++ack.session; inject(a,ack,740); assert(a.state().delivery == Delivery::Sending);
    --ack.session; ++ack.id; inject(a,ack,750); assert(a.state().delivery == Delivery::Sending);
    --ack.id; inject(a,ack,760); assert(a.state().delivery == Delivery::Delivered && !a.state().draft[0]);
    a.tick(5000); assert(ta.texts() == 2);
    compose(a,"E"); a.press(Button::Both,true,5010); assert(a.state().messageId == ack.id + 1);
    puts("PASS ACKs, retries, duplicate suppression, stale ACK rejection and message IDs");
}
void failuresAndHeartbeat() {
    Capture t; t.available=false; App app(t,8); compose(app,"T");
    const uint32_t start=UINT32_MAX-200; app.press(Button::Both,true,start);
    for (unsigned i=1;i<=4;++i) app.tick(start+AckTimeoutMs*i);
    assert(app.state().delivery == Delivery::Failed && app.state().attempts == 4 && strcmp(app.state().draft,"T") == 0);
    const auto id=app.state().messageId; app.press(Button::Both,true,4000);
    assert(app.state().messageId == id && app.state().attempts == 1);
    for (unsigned i=1;i<=4;++i) app.tick(4000+AckTimeoutMs*i);
    compose(app,"E",7000);
    Packet ack; ack.type=PacketType::Ack; ack.session=8; ack.id=id; inject(app,ack,7100);
    assert(app.state().delivery == Delivery::Delivered && strcmp(app.state().draft,"TE") == 0);
    Capture link; App peer(link,11); peer.tick(0);
    Packet pong; assert(deserialize(link.sent[0].data(),link.sent[0].size(),pong)); pong.type=PacketType::Pong;
    ++pong.session; inject(peer,pong,10); assert(!peer.state().peerConnected);
    --pong.session; inject(peer,pong,100); assert(peer.state().peerConnected);
    peer.tick(6100); assert(!peer.state().peerConnected);
    inject(peer,pong,6200); assert(!peer.state().peerConnected);
    puts("PASS draft retention, manual retry, timer wrap, late ACK and heartbeat challenge/timeout");
}
struct FailingCounters : MemoryCounters { bool commitRx(uint64_t, bool) override { return false; } };
void security() {
    uint8_t key[32]; for (unsigned i=0;i<32;++i) key[i]=i+1;
    MemoryCounters ac,bc; SecureChannel a(ac),b(bc); a.begin(key,0); b.begin(key,1);
    Packet p; p.session=1; p.id=42; strcpy(p.text,"SECRET");
    uint8_t plain[MaxPacket],wire[SecureSize],opened[MaxPacket];
    auto n=serialize(p,plain,sizeof(plain)); assert(a.seal(plain,n,wire) == SecureSize);
    for (size_t i=0;i<SecureSize;++i) for (unsigned bit=0;bit<8;++bit) {
        wire[i]^=1<<bit; assert(!b.open(wire,SecureSize,opened)); wire[i]^=1<<bit;
    }
    for (size_t size=0;size<SecureSize;++size) assert(!b.open(wire,size,opened));
    assert(!b.open(wire,SecureSize+1,opened));
    assert(b.open(wire,SecureSize,opened) == n && memcmp(plain,opened,n) == 0);
    assert(!b.open(wire,SecureSize,opened)); assert(!a.open(wire,SecureSize,opened));
    MemoryCounters wrongCounter; SecureChannel wrong(wrongCounter); key[0]^=1; wrong.begin(key,1);
    assert(!wrong.open(wire,SecureSize,opened)); key[0]^=1;
    SecureChannel rebooted(bc); rebooted.begin(key,1); assert(!rebooted.open(wire,SecureSize,opened));
    FailingCounters failed; SecureChannel noStorage(failed); noStorage.begin(key,1);
    assert(!noStorage.open(wire,SecureSize,opened));
    uint8_t retry[SecureSize]; assert(a.seal(plain,n,retry) == SecureSize && memcmp(wire,retry,SecureSize) != 0);
    assert(b.open(retry,SecureSize,opened) == n);
    p.type=PacketType::Ack; p.text[0]=0; n=serialize(p,plain,sizeof(plain));
    assert(b.seal(plain,n,wire) == SecureSize); wire[SecureHeader+5]^=1;
    assert(!a.open(wire,SecureSize,opened)); wire[SecureHeader+5]^=1; assert(a.open(wire,SecureSize,opened) == n);
    ac.tx=UINT64_MAX; assert(!a.seal(plain,n,wire));
    uint8_t zero[32]={}; SecureChannel unpaired(ac); unpaired.begin(zero,0); assert(!unpaired.seal(plain,n,wire));
    puts("PASS 872 bit-tamper checks, wrong keys, replay/reboot/reflection, encrypted ACKs, storage failure and exhausted counters");
}
void storageTests();
int main() {
    vectors(); codecs(); inputs(); compositionAndInbox(); deletionFeatures(); reliability(); failuresAndHeartbeat(); security(); usability(); journalTests(); screenLayouts();
    storageTests();
    puts("All 11 native test groups passed.");
}
