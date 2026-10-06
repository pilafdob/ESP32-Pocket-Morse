#pragma once
#include <stddef.h>
#include <stdint.h>
#include <vector>

namespace morse {
constexpr size_t MaxText = 64;
constexpr size_t MaxSequence = 6;
constexpr uint32_t AutoConfirmMs = 500;
constexpr uint32_t MultiTapMs = 430;
constexpr uint32_t DictionaryTapDelayMs = 800;
constexpr size_t HeaderSize = 17;
constexpr size_t MaxPacket = HeaderSize + MaxText;
constexpr uint32_t DebounceMs = 25;
constexpr uint32_t LongPressMs = 800;
constexpr uint32_t AckTimeoutMs = 700;
constexpr uint8_t MaxAttempts = 4;
constexpr uint32_t DisplayIdleMs = 60000;
constexpr uint32_t DeepSleepAfterUnlinkMs = 300000;
constexpr uint8_t ForcedDisplayIdleTaps = 5;
constexpr uint32_t ForcedDisplayIdleSequenceGapMs = 600;

char decode(const char* sequence);
const char* encode(char character);
size_t dictionaryCount();
char dictionarySymbol(size_t index);
const char* dictionaryCode(size_t index);
uint8_t signalBarsFromRssi(int8_t rssi);
bool batteryPercentFromMillivolts(uint32_t millivolts, uint8_t& percent);
class IBatteryInput {
public:
    virtual ~IBatteryInput() = default;
    virtual void setEnabled(bool enabled) = 0;
    // Millivolts at the board's ADC pin, before the board's 2:1 battery divider.
    virtual uint16_t readMillivolts() = 0;
};
class BatteryMonitor {
public:
    explicit BatteryMonitor(IBatteryInput& input) : input_(input) {}
    void begin(uint32_t now);
    // Non-blocking; true means a new valid or invalid reading was completed.
    bool tick(uint32_t now);
    void stop();
    bool valid() const { return valid_; }
    uint8_t percent() const { return percent_; }
private:
    enum class Phase : uint8_t { Idle, Settling, Sampling };
    IBatteryInput& input_;
    Phase phase_ = Phase::Idle;
    uint32_t nextSampleAt_ = 0, phaseAt_ = 0, sumMillivolts_ = 0;
    uint8_t sampleCount_ = 0, percent_ = 0;
    bool valid_ = false;
};
enum class PacketType : uint8_t { Text = 1, Ack = 2, Ping = 3, Pong = 4 };
struct Packet {
    PacketType type = PacketType::Text;
    uint64_t session = 0;
    uint32_t id = 0;
    char text[MaxText + 1] = {};
};
struct InboxMessage {
    uint64_t ordinal = 0, session = 0;
    uint32_t id = 0;
    char text[MaxText + 1] = {};
    bool unread = true;
};
enum class StoreResult : uint8_t { Stored, Duplicate, Full, Error };
class IInbox {
public:
    virtual ~IInbox() = default;
    virtual StoreResult append(uint64_t session, uint32_t id, const char* text) = 0;
    virtual uint32_t count() const = 0;
    virtual uint32_t unreadCount() const = 0;
    virtual uint32_t remaining() const = 0;
    virtual bool oldestUnread(InboxMessage& out) = 0;
    virtual bool next(uint64_t after, InboxMessage& out) = 0;
    virtual bool previous(uint64_t before, InboxMessage& out) = 0;
    virtual uint32_t position(uint64_t ordinal) = 0;
    virtual bool markRead(uint64_t ordinal) = 0;
    virtual bool erase(uint64_t ordinal) = 0;
};
// RAM implementation for host/WASM/Wokwi. Hardware attaches the flash journal.
class MemoryInbox : public IInbox {
public:
    explicit MemoryInbox(size_t capacity = 1024) : capacity_(capacity) {}
    StoreResult append(uint64_t session, uint32_t id, const char* text) override;
    uint32_t count() const override;
    uint32_t unreadCount() const override;
    uint32_t remaining() const override;
    bool oldestUnread(InboxMessage& out) override;
    bool next(uint64_t after, InboxMessage& out) override;
    bool previous(uint64_t before, InboxMessage& out) override;
    uint32_t position(uint64_t ordinal) override;
    bool markRead(uint64_t ordinal) override;
    bool erase(uint64_t ordinal) override;
private:
    std::vector<InboxMessage> messages_;
    size_t capacity_;
    uint64_t nextOrdinal_ = 1;
};
size_t serialize(const Packet& packet, uint8_t* bytes, size_t capacity);
bool deserialize(const uint8_t* bytes, size_t length, Packet& packet);

// Adapters only deliver bytes; application ACKs determine delivery, not radio callbacks.
class ITransport {
public:
    virtual ~ITransport() = default;
    virtual bool send(const uint8_t* bytes, size_t length) = 0;
};
enum class Button : uint8_t { Dot, Dash, Both };
enum class Delivery : uint8_t { Idle, Sending, Delivered, Failed };
const char* deliveryName(Delivery delivery);
enum class Mode : uint8_t { Compose, Incoming, Reading, Dictionary };
const char* modeName(Mode mode);

struct State {
    char sequence[MaxSequence + 1] = {};
    char draft[MaxText + 1] = {};
    char received[MaxText + 1] = {};
    char lastSent[MaxText + 1] = {};
    char notice[32] = {};
    Delivery delivery = Delivery::Idle;
    Mode mode = Mode::Compose;
    uint8_t attempts = 0;
    uint32_t receivedCount = 0;
    uint32_t duplicates = 0;
    uint32_t messageId = 0;
    bool peerConnected = false;
    uint8_t peerSignalBars = 0;
    uint8_t batteryPercent = 0;
    bool batteryValid = false;
    bool displayIdle = false;
    bool deepSleepRequested = false;
    bool cursorVisible = true;
    uint8_t progress = 0;
    uint8_t dictionaryPage = 0;
    uint32_t inboxCount = 0, unreadCount = 0, inboxPosition = 0, inboxRemaining = 0;
    uint64_t selectedOrdinal = 0;
};

class App {
public:
    App(ITransport& transport, uint64_t session);
    void attachInbox(IInbox& inbox);
    void press(Button button, bool longPress, uint32_t now);
    void tick(uint32_t now);
    void receive(const uint8_t* bytes, size_t length, uint32_t now);
    void setButtonProgress(uint8_t progress, bool held);
    void setPeerSignalBars(uint8_t bars);
    void setBatteryReading(bool valid, uint8_t percent);
    void forceDisplayIdle(uint32_t now);
    const State& state() const { return state_; }
private:
    void commit();
    void eraseLetter();
    void eraseWord();
    void resolveTaps();
    void sendMessage(uint32_t now);
    void attempt(uint32_t now);
    void notice(const char* message);
    void refreshInbox();
    bool openInbox();
    void showMessage(const InboxMessage& message);
    ITransport& transport_;
    uint64_t session_;
    uint32_t nextId_ = 1;
    uint32_t sentAt_ = 0;
    uint32_t pingAt_ = 0, peerAt_ = 0, pingId_ = 0;
    bool pingStarted_ = false;
    uint32_t lastActivityAt_ = 0, unlinkedAt_ = 0;
    bool peerEverConnected_ = false, unlinkTimerStarted_ = false;
    uint32_t symbolAt_ = 0, tapAt_ = 0;
    uint8_t tapCount_ = 0, buttonProgress_ = 0;
    bool buttonHeld_ = false;
    Packet pending_;
    State state_;
    MemoryInbox defaultInbox_;
    IInbox* inbox_ = &defaultInbox_;
    bool clearInboxArmed_ = false;
};

// Feed raw active-low GPIO readings (true = pressed), including in the simulator.
class InputManager {
public:
    void sample(bool dot, bool dash, uint32_t now, App& app);
private:
    struct Key {
        bool raw = false, stable = false, longFired = false;
        uint32_t changed = 0, pressedAt = 0;
    } keys_[2];
    bool chord_ = false, chordLong_ = false;
    uint32_t chordAt_ = 0;
    uint32_t lastDisplayIdleTapAt_ = 0;
    uint8_t displayIdleTapCount_ = 0;
};

class IScreen {
public:
    virtual ~IScreen() = default;
    virtual void clear(uint16_t color) = 0;
    virtual void text(int x, int y, const char* value, int size, uint16_t color) = 0;
    virtual void dial(int x, int y, uint8_t progress, uint16_t color) = 0;
    virtual void signalBars(int, int, uint8_t, uint16_t) {}
};
enum class ScreenLayout : uint8_t { Portrait, Landscape };
void render(const State& state, const char* name, IScreen& screen,
            ScreenLayout layout = ScreenLayout::Portrait);
}
