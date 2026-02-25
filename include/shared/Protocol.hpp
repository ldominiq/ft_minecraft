
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

struct NetPacketGroup final : public Packet {
	static constexpr PacketType ID = PacketType::GROUP;
	std::vector<std::vector<uint8_t>> rawPackets;

	NetPacketGroup() : Packet(ID) {}

	void add(const Packet& pkt) {
		rawPackets.push_back(encodePacket(pkt));
	}

	void encode(BufferWriter& w) const override {
		w.write_u16(static_cast<uint16_t>(rawPackets.size()));
		for (auto& raw : rawPackets) {
			w.write_u32(static_cast<uint32_t>(raw.size()));
			w.write_bytes(raw.data(), raw.size());
		}
	}

	void decode(BufferReader& r) override {
		auto count = r.read_u16();
		rawPackets.clear();
		rawPackets.reserve(count);
		for (uint16_t i = 0; i < count; ++i) {
			auto size = r.read_u32();
			auto bytes = r.read_bytes(size);
			rawPackets.push_back(std::move(bytes));
		}
	}

	// Utility: decode inner packets
	std::vector<PacketPtr> unpack() const {
		std::vector<PacketPtr> result;
		result.reserve(rawPackets.size());
		for (auto& raw : rawPackets) {
			result.push_back(decodePacket(raw.data(), raw.size()));
		}
		return result;
	}
};
inline AutoRegister<NetPacketGroup> _reg_NetPacketGroup;

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

struct NetDisconnect final : public Packet {
    static constexpr PacketType ID = PacketType::NET_DISCONNECT;
    std::string username;

    NetDisconnect() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
        w.write_string(username);
    }
    void decode(BufferReader& r) override {
        username = r.read_string();
    }
};
inline AutoRegister<NetDisconnect> _reg_NetDisconnect;

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
	uint8_t activeHotbarSlot = -1;
    float pitch = 0.0f;   // absolute rotation around X axis
    float yaw = 0.0f;     // absolute rotation around Y axis
	uint8_t loadRadius = 4; // maybe this should go elsewhere. Oh well!

    NetPlayerInputs() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
        w.write_u16(keys);
		w.write_u8(activeHotbarSlot);
        w.write_f32(pitch);
        w.write_f32(yaw);
		w.write_u8(loadRadius);
    }

    void decode(BufferReader& r) override {
        keys = r.read_u16();
		activeHotbarSlot = r.read_u8();
        pitch = r.read_f32();
        yaw = r.read_f32();
		loadRadius = r.read_u8();
    }
};
inline AutoRegister<NetPlayerInputs> _reg_NetPlayerInput;

struct NetPlayerMouseInputs final : public Packet {
    static constexpr PacketType ID = PacketType::PLAYER_MOUSE_INPUT;

	uint8_t mouseButtons = 0;	// bitfield
	//uint16_t itemID // itemID

    NetPlayerMouseInputs() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
        w.write_u8(mouseButtons);
    }

    void decode(BufferReader& r) override {
        mouseButtons = r.read_u8();
    }
};
inline AutoRegister<NetPlayerMouseInputs> _reg_NetPlayerMouseInput;

// TODO : add delta compression & put inside of a new Snapshot packet
struct NetPlayerMove final : public Packet {
	static constexpr PacketType ID = PacketType::PLAYER_MOVE;
	int32_t serverTick; // TODO : move to snapshot packet

	float positionX;
	float positionY;
	float positionZ;

	float velocityX;
	float velocityY;
	float velocityZ;

	float health; // Health shouldn't really be here as it should probably just be sent when it's updated. but it's whatever!

	NetPlayerMove() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
		w.write_i32(serverTick);
		w.write_f32(positionX);
		w.write_f32(positionY);
		w.write_f32(positionZ);
		w.write_f32(velocityX);
		w.write_f32(velocityY);
		w.write_f32(velocityZ);
		w.write_f32(health);
    }
    void decode(BufferReader& r) override {
		serverTick = r.read_i32();
		positionX = r.read_f32();
		positionY = r.read_f32();
		positionZ = r.read_f32();
		velocityX = r.read_f32();
		velocityY = r.read_f32();
		velocityZ = r.read_f32();
		health = r.read_f32();
    }
};
inline AutoRegister<NetPlayerMove> _reg_NetPlayerMove;


// TODO : add delta compression & put inside of a new Snapshot packet
// struct NetOnHit final : public Packet {
// 	static constexpr PacketType ID = PacketType::NET_ON_HIT;

// 	float health;

// 	NetOnHit() : Packet(ID) {}

//     void encode(BufferWriter& w) const override {
// 		w.write_f32(health);
//     }
//     void decode(BufferReader& r) override {
// 		health = r.read_f32();
//     }
// };
// inline AutoRegister<NetOnHit> _reg_NetOnHit;

struct NetEntityMove final : public Packet {
	static constexpr PacketType ID = PacketType::NET_ENTITY_MOVE;

	EEntityTypes eEntityType;
	uint32_t entityID;
	uint16_t type = 0;	// stone/dirt/etc.. for block - zombie/creeper/etc... for living entity. -1 to erase the entity

	//position
	float positionX;
	float positionY;
	float positionZ;

	float yaw;

	NetEntityMove() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
		w.write_u8(eEntityType);
		w.write_u32(entityID);
		w.write_u16(type);
		w.write_f32(positionX);
		w.write_f32(positionY);
		w.write_f32(positionZ);
		w.write_f32(yaw);
    }
    void decode(BufferReader& r) override {
		eEntityType = static_cast<EEntityTypes>(r.read_u8());
		entityID = r.read_u32();
		type = r.read_u16();
		positionX = r.read_f32();
		positionY = r.read_f32();
		positionZ = r.read_f32();
		yaw = r.read_f32();
    }
};
inline AutoRegister<NetEntityMove> _reg_NetEntityMove;

struct NetInventory final : public Packet {
	static constexpr PacketType ID = PacketType::NET_INVENTORY;

	uint16_t type = 0;
	int16_t amount = 0;
	uint8_t slot = 0;

	NetInventory() : Packet(ID) {}

	void encode(BufferWriter& w) const override {
		w.write_u16(type);
		w.write_i16(amount);
		w.write_u8(slot);
    }

	void decode(BufferReader& r) override {
		type = r.read_u16();
		amount = r.read_i16();
		slot = r.read_u8();
    }
};
inline AutoRegister<NetInventory> _reg_NetInventory;

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

//used in chat
struct NetMessage final : public Packet {
	static constexpr PacketType ID = PacketType::NET_MESSAGE;
	std::string message;
	
	NetMessage() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
		w.write_string(message);
    }
    void decode(BufferReader& r) override {
		message = r.read_string();
    }
};
inline AutoRegister<NetMessage> _reg_NetServerMessage;

struct NetImGui final : public Packet {
    static constexpr PacketType ID = PacketType::NET_IMGUI;

	uint8_t currentBiome;

    NetImGui() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
        w.write_u8(currentBiome);
    }

    void decode(BufferReader& r) override {
        currentBiome = r.read_u8();
    }
};
inline AutoRegister<NetImGui> _reg_NetImGui;

#endif
