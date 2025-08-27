
#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include "Network.hpp"

// NET_CONNECT = 1,  // C2S
// NET_ACCEPT,       // S2C
// PLAYER_INPUT,     // C2S
// PLAYER_MOVEMENT,  // S2C
// CHUNK_HEADER,     // S2C
// CHUNK_DATA,       // S2C
// NET_DISCONNECT,   // C2S

// --- Concrete packets ---
struct NetConnect final : public Packet {
    static constexpr PacketType ID = PacketType::NET_CONNECT;
    std::string username;

    NetConnect() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
        w.write_string(username);
    }
    void decode(BufferReader& r) override {
        username = r.read_string();
    }
};
inline AutoRegister<NetConnect> _reg_NetConnect;

struct NetAccept final : public Packet {
    static constexpr PacketType ID = PacketType::NET_ACCEPT;
    uint32_t clientId = 0;

    NetAccept() : Packet(ID) {}

    void encode(BufferWriter& w) const override { w.write_u32(clientId); }
    void decode(BufferReader& r) override { clientId = r.read_u32(); }
};
inline AutoRegister<NetAccept> _reg_NetAccept;

struct NetPlayerInputs final : public Packet {
    static constexpr PacketType ID = PacketType::PLAYER_INPUT;

	uint16_t keys = 0;	// bitfield
    float pitch = 0.0f;   // absolute rotation around X axis
    float yaw = 0.0f;     // absolute rotation around Y axis
	uint8_t loadRadius = 4; // maybe this should go elsewhere. Oh well!

    NetPlayerInputs() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
        w.write_u16(keys);
        w.write_f32(pitch);
        w.write_f32(yaw);
		w.write_u8(loadRadius);
    }

    void decode(BufferReader& r) override {
        keys = r.read_u16();
        pitch = r.read_f32();
        yaw = r.read_f32();
		loadRadius = r.read_u8();
    }
};
inline AutoRegister<NetPlayerInputs> _reg_NetPlayerInput;

struct NetPlayerMouseInputs final : public Packet {
    static constexpr PacketType ID = PacketType::PLAYER_MOUSE_INPUT;

	uint8_t mouseButtons = 0;	// bitfield
	//uint16_t itemID // itemIDK

    NetPlayerMouseInputs() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
        w.write_u8(mouseButtons);
    }

    void decode(BufferReader& r) override {
        mouseButtons = r.read_u8();
    }
};
inline AutoRegister<NetPlayerMouseInputs> _reg_NetPlayerMouseInput;

// TODO : add delta compression
struct NetPlayerMove final : public Packet {
	static constexpr PacketType ID = PacketType::PLAYER_MOVE;
	float positionX;
	float positionY;
	float positionZ;

	NetPlayerMove() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
		w.write_f32(positionX);
		w.write_f32(positionY);
		w.write_f32(positionZ);
    }
    void decode(BufferReader& r) override {
		positionX = r.read_f32();
		positionY = r.read_f32();
		positionZ = r.read_f32();
    }
};
inline AutoRegister<NetPlayerMove> _reg_NetPlayerMove;

struct NetChunkHeader final : public Packet {
    static constexpr PacketType ID = PacketType::CHUNK_HEADER;
	int32_t X = 0;
	int32_t Z = 0;
    uint32_t uncompressedSize = 0;
    uint32_t compressedSize   = 0;

    NetChunkHeader() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
		w.write_i32(X);
		w.write_i32(Z);
        w.write_u32(uncompressedSize);
        w.write_u32(compressedSize);
    }
    void decode(BufferReader& r) override {
		X = r.read_i32();
		Z = r.read_i32();
        uncompressedSize = r.read_u32();
        compressedSize   = r.read_u32();
    }
};
inline AutoRegister<NetChunkHeader> _reg_NetChunkHeader;


// THIS WAS FOR DEBUG
// #include <iomanip>
// inline void printHex(const std::vector<uint8_t>& data, size_t max = 64) {
//     size_t n = std::min(data.size(), max);
//     for (size_t i = 0; i < n; ++i) {
//         std::cout << std::hex << std::setw(2) << std::setfill('0')
//                   << static_cast<int>(data[i]) << " ";
//     }
//     if (data.size() > max) std::cout << "...";
//     std::cout << std::dec << "\n";
// }

// Example CHUNK_DATA carrying raw bytes (length-prefixed)
struct NetChunkData final : public Packet {
    static constexpr PacketType ID = PacketType::CHUNK_DATA;
	int32_t X = 0;
	int32_t Z = 0;
    std::vector<uint8_t> data;

    NetChunkData() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
		w.write_i32(X);
		w.write_i32(Z);
        if (data.size() > MAXLINE) throw std::runtime_error("chunk too large");
        w.write_u32(static_cast<uint32_t>(data.size()));
        if (!data.empty()) w.write_bytes(data.data(), data.size());
    }
    void decode(BufferReader& r) override {
		X = r.read_i32();
		Z = r.read_i32();
        uint32_t len = r.read_u32();
        data = r.read_bytes(len);
    }
};
inline AutoRegister<NetChunkData> _reg_NetChunkData;

struct NetModifiedBlockData final : public Packet {
	static constexpr PacketType ID = PacketType::MODIFIED_BLOCK_DATA;
	int32_t x = 0;
	int32_t y = 0;
	int32_t z = 0;
	uint8_t blockType;
	
	NetModifiedBlockData() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
		w.write_i32(x);
		w.write_i32(y);
		w.write_i32(z);
		w.write_u8(blockType);
    }
    void decode(BufferReader& r) override {
		x = r.read_i32();
		y = r.read_i32();
		z = r.read_i32();
		blockType = r.read_u8();
    }
};
inline AutoRegister<NetModifiedBlockData> _reg_NetModifiedBlockData;

#endif
