
#ifndef NETWORK_HPP
#define NETWORK_HPP

#include <cstdint>
#include <vector>
#include <string>
#include <functional>
#include <memory>
#include <array>
#include <stdexcept>
#include <type_traits>
#include <cstring>
#include "Config.hpp"

#define MAXLINE 1400

//For no reason at all half the messages start with NET and the others do not
enum class PacketType : uint8_t {
	NET_CONNECT = 1,		// C2S
	NET_ACCEPT,				// S2C
	PLAYER_INPUT,			// C2S
	PLAYER_MOUSE_INPUT,		// C2S
	PLAYER_MOVE,			// S2C
	NET_ENTITY_MOVE,		// S2C TODO : put it inside a group and send multiple at once.
	NET_INVENTORY,			// S2C
	NET_INVENTORY_ACTION,	// C2S
	CHUNK_HEADER,			// S2C
	CHUNK_DATA,				// S2C
	MODIFIED_BLOCK_DATA,	// S2C
	NET_DISCONNECT,			// C2S
	NET_MESSAGE,			// S2C && C2S
    NET_IMGUI,          	// S2C

	GROUP,					// for grouped packets
};

enum class PacketFlags : uint8_t {
    None            = 0,
    Vita            = 1 << 0,
    Compressed      = 1 << 1,
    FinalChunk      = 1 << 2,
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
	IN_DOWN			= 1 << 5,
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
	INV_LEFT_CLICK,          // normal click
	INV_RIGHT_CLICK,
	INV_SHIFT_CLICK,
	INV_DRAG_BEGIN,
	INV_DRAG_ADD,       // add slot to drag selection
	INV_DRAG_END,
	INV_DROP_CURSOR,    // click outside inventory
	//   OPEN_CONTAINER could be useful for other inventories like chests etc...
	//   CLOSE_CONTAINER 
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
	void write_i32(int32_t v) {
		write_u32(static_cast<uint32_t>(v));
	}

	void write_f32(float v) {
		static_assert(sizeof(float) == 4, "float must be 32-bit IEEE 754");
		uint32_t bits;
		std::memcpy(&bits, &v, 4);   // preserve exact bit pattern
		write_u32(bits);             // already writes big-endian
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
    uint16_t sequence = 0;
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
    w.write_u16(pkt.sequence);
    w.write_u8(static_cast<uint8_t>(to_under(pkt.flags)));
    pkt.encode(w);              // payload
    return w.take();
}

inline PacketPtr decodePacket(const uint8_t* data, size_t size){
    BufferReader r(data, size);
    auto type_u = r.read_u8();
    auto seq    = r.read_u16();
    auto flags  = r.read_u8();

    auto& factory = g_registry[type_u];
    if (!factory) throw std::runtime_error("unknown packet type");

    auto pkt = factory();
    pkt->sequence = seq;
    pkt->flags    = static_cast<PacketFlags>(flags);
    pkt->decode(r);

    // Optional: ensure no trailing bytes
    // if (!r.empty()) throw std::runtime_error("trailing bytes");
    return pkt;
}

#endif
