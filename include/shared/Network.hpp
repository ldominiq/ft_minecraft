
#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <array>
#include <map>
#include <chrono>
#include <optional>
#include <utility>
#include <stdexcept>
#include <type_traits>
#include <cstring>
#include "Config.hpp"

#define MAXLINE 1400

//For no reason at all half the messages start with NET and the others do not
enum class PacketType : uint8_t {
	NET_CONNECT = 1,		// C2S
	NET_ACCEPT,				// S2C
	NET_SET_NAME,			// S2C set name when there's already an existing player with the same name.
	PLAYER_INPUT,			// C2S
	PLAYER_MOUSE_INPUT,		// C2S
	PLAYER_MOVE,			// S2C
	PLAYER_GAMEMODE,		// S2C
	NET_ENTITY_MOVE,		// S2C TODO : put it inside a group and send multiple at once.
	NET_INVENTORY,			// S2C
	NET_INVENTORY_ACTION,	// C2S
	CHUNK_HEADER,			// S2C
	CHUNK_DATA,				// S2C
	MODIFIED_BLOCK_DATA,	// S2C
	NET_DISCONNECT,			// C2S
	NET_MESSAGE,			// S2C && C2S
    NET_IMGUI,          	// S2C
    NET_SKY_TIME,           // S2C && C2S
	NET_TERRAIN_PARAMS,		// C2S && S2C (for syncing terrain generation parameters)
	NET_PING,				// C2S && S2C (for latency measurement)
	NET_PONG,				// S2C && C2S (response to ping)
	NET_PLAYER_PING,		// C2S: client reports its measured ping to server
	NET_PING_LIST,			// S2C: server broadcasts all players pings

	GROUP,					// for grouped packets
	RELIABLE_NACK,			// receiver-driven retransmit request (from,to inclusive)
};

enum class PacketFlags : uint8_t {
    None            = 0,
    Vita            = 1 << 0,
    Compressed      = 1 << 1,
    FinalChunk      = 1 << 2,
    Reliable        = 1 << 3, // participates in per-direction sequence layer
};

enum EEntityTypes : uint8_t {
	ITEMS = 0,
	LIVING_ENTITIES,
};

enum Inputs : uint16_t {
	IN_FORWARD		= 1 << 0,
	IN_BACKWARD		= 1 << 1,
	IN_LEFT			= 1 << 2,
	IN_RIGHT		= 1 << 3,
	IN_UP			= 1 << 4, //jump
	IN_SNEAK			= 1 << 5,
	IN_RUN			= 1 << 6,
	IN_DROP			= 1 << 7,

	// IN_TOGGLE_UI	= 1 << 10, // e.g. F4
	// … up to 16 for uint16_t, or expand to uint32_t later
};

enum MouseInputs : uint8_t {
	IN_LEFT_CLICK	= 1 << 0,  // left click
	IN_RIGHT_CLICK	= 1 << 1,  // right click
};


enum InventoryActionType : uint8_t {
	INV_LEFT_CLICK,
	INV_RIGHT_CLICK,
};

enum InventoryModifiers : uint8_t {
	INV_DOUBLE_CLICK,
	INV_SHIFT,
	INV_DRAG_BEGIN,
	INV_DRAG_ADD,       // add slot to drag selection
	INV_DRAG_CANCEL,
	INV_DRAG_END,
	INV_DROP_CURSOR,    // click outside inventory
};

inline PacketFlags operator|(PacketFlags a, PacketFlags b){
    return static_cast<PacketFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}
inline PacketFlags operator&(PacketFlags a, PacketFlags b) {
    return static_cast<PacketFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
}
inline bool hasFlag(PacketFlags f, PacketFlags bit){
    return (static_cast<uint8_t>(f) & static_cast<uint8_t>(bit)) != 0;
}

template<class E>
constexpr auto to_under(E e) noexcept {
    return static_cast<std::underlying_type_t<E>>(e);
}

//TODO: Change this for an int packer instead
// --- Buffer primitives (network byte order: big-endian) ---
struct BufferWriter {
    std::vector<uint8_t> buf;

    void write_u8(uint8_t v){ buf.push_back(v); }

    void write_u16(uint16_t v){
        buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(v & 0xFF));
    }
	void write_i16(int16_t v) {
		write_u16(static_cast<uint16_t>(v));
	}

    void write_u32(uint32_t v){
        buf.push_back(static_cast<uint8_t>((v >> 24) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 16) & 0xFF));
        buf.push_back(static_cast<uint8_t>((v >> 8) & 0xFF));
        buf.push_back(static_cast<uint8_t>(v & 0xFF));
    }

    void write_u64(uint64_t v) {
        write_u32(static_cast<uint32_t>(v >> 32));
        write_u32(static_cast<uint32_t>(v & 0xFFFFFFFFu));
    }

	void write_i32(int32_t v) {
		write_u32(static_cast<uint32_t>(v));
	}

	void write_f32(float v) {
		static_assert(sizeof(float) == 4, "float must be 32-bit IEEE 754");
		uint32_t bits;
		std::memcpy(&bits, &v, 4);   // preserve exact bit pattern
		write_u32(bits);             // already writes big-endian
	}

	void write_f64(double v) {
		static_assert(sizeof(double) == 8, "double must be 64-bit IEEE 754");
		uint64_t bits;
		std::memcpy(&bits, &v, 8);
		write_u64(bits);
	}

    void write_bytes(const uint8_t* p, size_t n){
        buf.insert(buf.end(), p, p + n);
    }
    void write_string(const std::string& s){
        if (s.size() > 0xFFFF) throw std::runtime_error("string too long");
        write_u16(static_cast<uint16_t>(s.size()));
        write_bytes(reinterpret_cast<const uint8_t*>(s.data()), s.size());
    }

    std::vector<uint8_t> take(){ return buf; }
};

struct BufferReader {
    const uint8_t* p = nullptr;
    size_t n = 0;
    size_t off = 0;

    BufferReader(const uint8_t* data, size_t size): p(data), n(size) {}

    void need(size_t k) {
        if (off + k > n) throw std::runtime_error("buffer underflow");
    }
    uint8_t read_u8(){ need(1); return p[off++]; }
    uint16_t read_u16(){
        need(2);
        uint16_t v = (static_cast<uint16_t>(p[off]) << 8) | p[off+1];
        off += 2; return v;
    }
	int16_t read_i16() {
		return static_cast<int16_t>(read_u16());
	}
    uint32_t read_u32(){
        need(4);
        uint32_t v = (static_cast<uint32_t>(p[off]) << 24) |
                     (static_cast<uint32_t>(p[off+1]) << 16) |
                     (static_cast<uint32_t>(p[off+2]) << 8) |
                      static_cast<uint32_t>(p[off+3]);
        off += 4; return v;
    }

    uint64_t read_u64() {
        uint64_t hi = read_u32();
		uint64_t lo = read_u32();
		return (hi << 32) | lo;
    }

	int32_t read_i32() {
		uint32_t v = read_u32();          // read 4 bytes as big-endian unsigned
		return static_cast<int32_t>(v);   // reinterpret as signed
	}

	float read_f32() {
		uint32_t bits = read_u32();  // read big-endian u32
		float v;
		std::memcpy(&v, &bits, 4);   // reconstruct the float
		return v;
	}

	double read_f64() {
		uint64_t bits = read_u64();
		double v;
		std::memcpy(&v, &bits, 8);
		return v;
	}

    std::string read_string(){
        uint16_t len = read_u16();
        need(len);
        std::string s(reinterpret_cast<const char*>(p + off), len);
        off += len;
        return s;
    }
    std::vector<uint8_t> read_bytes(size_t len){
        need(len);
        std::vector<uint8_t> out(p + off, p + off + len);
        off += len; return out;
    }
    bool empty() const { return off >= n; }
};

// --- Packet base ---
struct Packet {
    PacketType type;
    uint32_t reliableSeq = 0; // only meaningful if flags has Reliable
    PacketFlags flags = PacketFlags::None;

    explicit Packet(PacketType t) : type(t) {}
    virtual ~Packet() = default;

    // payload-only encode/decode (no header here)
    virtual void encode(BufferWriter& w) const = 0;
    virtual void decode(BufferReader& r) = 0;
};

// --- Registry ---
using PacketPtr = std::unique_ptr<Packet>;
using PacketFactory = std::function<PacketPtr()>;
inline std::array<PacketFactory, 256> g_registry;

template<class P>
struct AutoRegister {
    AutoRegister(){
        g_registry[to_under(P::ID)] = [](){ return std::make_unique<P>(); };
    }
};

// --- Top-level encode/decode including header ---
inline std::vector<uint8_t> encodePacket(const Packet& pkt){
    BufferWriter w;
    w.write_u8(static_cast<uint8_t>(to_under(pkt.type)));
    w.write_u32(pkt.reliableSeq);
    w.write_u8(static_cast<uint8_t>(to_under(pkt.flags)));
    pkt.encode(w);              // payload
    return w.take();
}

inline PacketPtr decodePacket(const uint8_t* data, size_t size){
    BufferReader r(data, size);
    auto type_u = r.read_u8();
    auto seq    = r.read_u32();
    auto flags  = r.read_u8();

    auto& factory = g_registry[type_u];
    if (!factory) throw std::runtime_error("unknown packet type");

    auto pkt = factory();
    pkt->reliableSeq = seq;
    pkt->flags       = static_cast<PacketFlags>(flags);
    pkt->decode(r);

    // Optional: ensure no trailing bytes
    // if (!r.empty()) throw std::runtime_error("trailing bytes");
    return pkt;
}

// --- Reliability layer ---
//
// Receiver-driven NACK-on-gap scheme. Each direction has its own monotonic
// sequence counter starting at 0. Packets opt in via PacketFlags::Reliable.
// Sender buffers a bounded history so it can retransmit on NACK. Receiver
// delivers in order, buffering out-of-order packets and sending NACKs for
// missing ranges. A periodic "I'm stuck" NACK handles tail-drop.
//
// Unreliable packets (inputs, movement, ping, sky time...) pass through
// untouched: no seq, no buffering, no retransmit.

struct ReliabilitySender {
    static constexpr size_t kMaxBuffered = 512;
    uint32_t nextSeq = 0;
    std::map<uint32_t, std::vector<uint8_t>> buffer; // seq -> encoded bytes

    ReliabilitySender() = default;
    ReliabilitySender(const ReliabilitySender&) = delete;
    ReliabilitySender& operator=(const ReliabilitySender&) = delete;
    ReliabilitySender(ReliabilitySender&&) noexcept = default;
    ReliabilitySender& operator=(ReliabilitySender&&) noexcept = default;
};

struct ReliabilityReceiver {
    // Bounds on the reorder buffer: a peer can't make us hold more than
    // kMaxReorderBuffered out-of-order packets, and can't queue a packet
    // more than kMaxReorderWindow seqs past what we're waiting for.
    // Without these, a peer can force unbounded memory growth (DoS).
    static constexpr size_t kMaxReorderBuffered = 512;
    static constexpr uint32_t kMaxReorderWindow = 4096;

    uint32_t expectedSeq = 0;
    std::map<uint32_t, PacketPtr> reorderBuffer;
    std::chrono::steady_clock::time_point lastNackSent{};
    std::chrono::steady_clock::time_point lastProgressAt = std::chrono::steady_clock::now();

    // reorderBuffer holds unique_ptr<Packet>, so copy is nonsensical. Spell it
    // out so the compiler never tries to instantiate map<...>::map(const map&).
    ReliabilityReceiver() = default;
    ReliabilityReceiver(const ReliabilityReceiver&) = delete;
    ReliabilityReceiver& operator=(const ReliabilityReceiver&) = delete;
    ReliabilityReceiver(ReliabilityReceiver&&) noexcept = default;
    ReliabilityReceiver& operator=(ReliabilityReceiver&&) noexcept = default;
};

// Stamp a Reliable packet with the next seq and buffer its encoded bytes so
// it can be resent on NACK. Non-reliable packets are encoded straight through.
// Returns the bytes to hand to sendto().
inline std::vector<uint8_t> reliabilityStamp(ReliabilitySender& s, Packet& pkt) {
    if (!hasFlag(pkt.flags, PacketFlags::Reliable)) {
        return encodePacket(pkt);
    }
    pkt.reliableSeq = s.nextSeq++;
    auto bytes = encodePacket(pkt);
    s.buffer[pkt.reliableSeq] = bytes;
    while (s.buffer.size() > ReliabilitySender::kMaxBuffered) {
        s.buffer.erase(s.buffer.begin()); // drop oldest seq past the window
    }
    return bytes;
}

struct ReliabilityIngest {
    std::vector<PacketPtr> ready; // packets ready to dispatch, in order
    std::optional<std::pair<uint32_t, uint32_t>> nack; // (from,to) inclusive
};

// Absorb a decoded incoming packet. Returns which packets are ready for
// dispatch (one or more, in order) and optionally a NACK range to send back.
// Type filtering (e.g. RELIABLE_NACK) is the caller's responsibility: this
// function passes packets through once they are in order.
inline ReliabilityIngest reliabilityIngest(
    ReliabilityReceiver& r,
    PacketPtr pkt,
    std::chrono::steady_clock::time_point now
) {
    ReliabilityIngest out;
    if (!pkt) return out;
    if (!hasFlag(pkt->flags, PacketFlags::Reliable)) {
        out.ready.push_back(std::move(pkt));
        return out;
    }
    const uint32_t seq = pkt->reliableSeq;
    if (seq < r.expectedSeq) {
        return out; // duplicate
    }
    if (seq == r.expectedSeq) {
        out.ready.push_back(std::move(pkt));
        r.expectedSeq++;
        r.lastProgressAt = now;
        for (;;) {
            auto it = r.reorderBuffer.find(r.expectedSeq);
            if (it == r.reorderBuffer.end()) break;
            out.ready.push_back(std::move(it->second));
            r.reorderBuffer.erase(it);
            r.expectedSeq++;
        }
        return out;
    }
    // seq > expected: hole detected. Drop packets wildly past the window so a
    // peer can't waste a slot with a giant seq, then evict from the tail if
    // the buffer would overflow - packets near expectedSeq are what unblocks
    // the stream, so we keep those and shed the furthest-ahead ones.
    if (seq - r.expectedSeq > ReliabilityReceiver::kMaxReorderWindow) {
        return out;
    }
    r.reorderBuffer[seq] = std::move(pkt);
    while (r.reorderBuffer.size() > ReliabilityReceiver::kMaxReorderBuffered) {
        r.reorderBuffer.erase(std::prev(r.reorderBuffer.end()));
    }
    constexpr auto kNackRateLimit = std::chrono::milliseconds(100);
    if (now - r.lastNackSent >= kNackRateLimit) {
        out.nack = std::make_pair(r.expectedSeq, seq - 1);
        r.lastNackSent = now;
    }
    return out;
}

// Collect encoded bytes to resend in response to an incoming NACK.
// Caller sendto's each of these directly (they already have correct seq).
inline std::vector<const std::vector<uint8_t>*> reliabilityOnNack(
    const ReliabilitySender& s, uint32_t from, uint32_t to, size_t cap = 128
) {
    std::vector<const std::vector<uint8_t>*> out;
    auto it = s.buffer.lower_bound(from);
    for (; it != s.buffer.end() && it->first <= to && out.size() < cap; ++it) {
        out.push_back(&it->second);
    }
    return out;
}

// Per-tick: true if we should send a "catch me up" NACK(expected,expected)
// because no reliable progress has arrived for a while. Updates lastProgressAt
// so we don't spam.
inline bool reliabilityShouldKeepalive(
    ReliabilityReceiver& r, std::chrono::steady_clock::time_point now
) {
    constexpr auto kIdle = std::chrono::seconds(1);
    if (now - r.lastProgressAt >= kIdle) {
        r.lastProgressAt = now;
        return true;
    }
    return false;
}

#endif
