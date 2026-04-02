#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include "Network.hpp"
#include "TerrainParams.hpp"

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


	NetPlayerMove() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
		w.write_i32(serverTick);
		w.write_f32(positionX);
		w.write_f32(positionY);
		w.write_f32(positionZ);
		w.write_f32(velocityX);
		w.write_f32(velocityZ);
		w.write_f32(velocityY);
    }
    void decode(BufferReader& r) override {
		serverTick = r.read_i32();
		positionX = r.read_f32();
		positionY = r.read_f32();
		positionZ = r.read_f32();
		velocityX = r.read_f32();
		velocityZ = r.read_f32();
		velocityY = r.read_f32();
    }
};
inline AutoRegister<NetPlayerMove> _reg_NetPlayerMove;

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
	uint8_t amount = 0;
	uint8_t slot = 0;	// HAND_ID for hand (37)

	NetInventory() : Packet(ID) {}

	void encode(BufferWriter& w) const override {
		w.write_u16(type);
		w.write_u8(amount);
		w.write_u8(slot);
    }

	void decode(BufferReader& r) override {
		type = r.read_u16();
		amount = r.read_u8();
		slot = r.read_u8();
    }
};
inline AutoRegister<NetInventory> _reg_NetInventory;

struct NetInventoryAction final : public Packet {
    static constexpr PacketType ID = PacketType::NET_INVENTORY_ACTION;

    uint8_t actionType = 0;
    uint8_t slot = 0;

    NetInventoryAction() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
        w.write_u8(actionType);
        w.write_u8(slot);
    }

    void decode(BufferReader& r) override {
        actionType = r.read_u8();
        slot = r.read_u8();
    }
};
inline AutoRegister<NetInventoryAction> _reg_NetInventoryAction;

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

	uint8_t currentBiome = 0;
	int32_t terrainHeight = 0;
	int32_t seaLevel = 64;
	int32_t worldSeed = 0;
	float continentalness = 0.0f;
	float erosion = 0.0f;
	float peakValley = 0.0f;
	float temperature = 0.0f;
	float humidity = 0.0f;

    NetImGui() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
        w.write_u8(currentBiome);
        w.write_i32(terrainHeight);
        w.write_i32(seaLevel);
        w.write_i32(worldSeed);
        w.write_f32(continentalness);
        w.write_f32(erosion);
        w.write_f32(peakValley);
        w.write_f32(temperature);
        w.write_f32(humidity);
    }

    void decode(BufferReader& r) override {
        currentBiome = r.read_u8();
        terrainHeight = r.read_i32();
        seaLevel = r.read_i32();
        worldSeed = r.read_i32();
        continentalness = r.read_f32();
        erosion = r.read_f32();
        peakValley = r.read_f32();
        temperature = r.read_f32();
        humidity = r.read_f32();
    }
};
inline AutoRegister<NetImGui> _reg_NetImGui;

// Terrain generation parameters sync packet (C2S && S2C)
struct NetTerrainParams final : public Packet {
    static constexpr PacketType ID = PacketType::NET_TERRAIN_PARAMS;

    int32_t seed = 1337;
    int32_t seaLevel = 64;
    int32_t bedrockLevel = 0;

    // River carving params
    float riverFrequency = 0.0048f;
    int32_t riverOctaves = 4;
    float riverPersistence = 0.5f;
    float riverLacunarity = 2.0f;
    float riverWidth = 0.030f;
    float riverBankFeather = 0.060f;
    float riverDepth = 18.0f;
    float riverWarpFrequency = 0.0012f;
    float riverWarpStrength = 180.0f;
    float riverMinContinentalness = -0.04f;
    float riverMaxContinentalness = 0.8f;

    // Lake carving params
    float lakeFrequency = 0.0010f;
    int32_t lakeOctaves = 3;
    float lakePersistence = 0.5f;
    float lakeLacunarity = 2.0f;
    float lakeThreshold = 0.62f;
    float lakeFeather = 0.14f;
    float lakeDepth = 10.0f;
    float lakeMinContinentalness = -0.02f;
    float lakeMaxContinentalness = 0.6f;

    // Heightmap dump settings
    int32_t genSize = 1000;
    int32_t downsample = 16;

    // Continentalness noise params
    float continentalnessFrequency = 0.001f;
    int32_t continentalnessOctaves = 5;
    float continentalnessPersistence = 0.245f;
    float continentalnessLacunarity = 3.250f;
    float continentalnessScalingFactor = 4.5f;

    // Erosion noise params
    float erosionFrequency = 0.009f;
    int32_t erosionOctaves = 5;
    float erosionPersistence = 0.35f;
    float erosionLacunarity = 2.37f;
    float erosionScalingFactor = 2.0f;

    // Peak / valley noise params
    float peakValleyFrequency = 0.001f;
    int32_t peakValleyOctaves = 5;
    float peakValleyPersistence = 0.271f;
    float peakValleyLacunarity = 1.438f;
    float peakValleyScalingFactor = 2.5f;

    // Temperature noise params
    float temperatureFrequency = 0.0012f;
    int32_t temperatureOctaves = 4;
    float temperaturePersistence = 0.50f;
    float temperatureLacunarity = 2.0f;
    float temperatureScalingFactor = 0.5f;

    // Humidity noise params
    float humidityFrequency = 0.0015f;
    int32_t humidityOctaves = 4;
    float humidityPersistence = 0.50f;
    float humidityLacunarity = 2.0f;
    float humidityScalingFactor = 0.5f;

    // Biome params
    int32_t biomeScaleChunks = 8;
    bool snapClimateToCells = true;
    float climateWarpFrequency = 0.0008f;
    float climateWarpStrength = 180.0f;

    float desertMoistureThreshold = 0.30f;
    float forestMoistureThreshold = 0.60f;
    float snowTemperatureThreshold = 0.28f;

    bool debugOresOnly = false;

    NetTerrainParams() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
        w.write_i32(seed);
        w.write_i32(seaLevel);
        w.write_i32(bedrockLevel);
        w.write_f32(riverFrequency);
        w.write_i32(riverOctaves);
        w.write_f32(riverPersistence);
        w.write_f32(riverLacunarity);
        w.write_f32(riverWidth);
        w.write_f32(riverBankFeather);
        w.write_f32(riverDepth);
        w.write_f32(riverWarpFrequency);
        w.write_f32(riverWarpStrength);
        w.write_f32(riverMinContinentalness);
        w.write_f32(riverMaxContinentalness);
        w.write_f32(lakeFrequency);
        w.write_i32(lakeOctaves);
        w.write_f32(lakePersistence);
        w.write_f32(lakeLacunarity);
        w.write_f32(lakeThreshold);
        w.write_f32(lakeFeather);
        w.write_f32(lakeDepth);
        w.write_f32(lakeMinContinentalness);
        w.write_f32(lakeMaxContinentalness);
        w.write_i32(genSize);
        w.write_i32(downsample);
        w.write_f32(continentalnessFrequency);
        w.write_i32(continentalnessOctaves);
        w.write_f32(continentalnessPersistence);
        w.write_f32(continentalnessLacunarity);
        w.write_f32(continentalnessScalingFactor);
        w.write_f32(erosionFrequency);
        w.write_i32(erosionOctaves);
        w.write_f32(erosionPersistence);
        w.write_f32(erosionLacunarity);
        w.write_f32(erosionScalingFactor);
        w.write_f32(peakValleyFrequency);
        w.write_i32(peakValleyOctaves);
        w.write_f32(peakValleyPersistence);
        w.write_f32(peakValleyLacunarity);
        w.write_f32(peakValleyScalingFactor);
        w.write_f32(temperatureFrequency);
        w.write_i32(temperatureOctaves);
        w.write_f32(temperaturePersistence);
        w.write_f32(temperatureLacunarity);
        w.write_f32(temperatureScalingFactor);
        w.write_f32(humidityFrequency);
        w.write_i32(humidityOctaves);
        w.write_f32(humidityPersistence);
        w.write_f32(humidityLacunarity);
        w.write_f32(humidityScalingFactor);
        w.write_i32(biomeScaleChunks);
        w.write_u8(snapClimateToCells ? 1 : 0);
        w.write_f32(climateWarpFrequency);
        w.write_f32(climateWarpStrength);
        w.write_f32(desertMoistureThreshold);
        w.write_f32(forestMoistureThreshold);
        w.write_f32(snowTemperatureThreshold);
        w.write_u8(debugOresOnly ? 1 : 0);
    }

    void decode(BufferReader& r) override {
        seed = r.read_i32();
        seaLevel = r.read_i32();
        bedrockLevel = r.read_i32();
        riverFrequency = r.read_f32();
        riverOctaves = r.read_i32();
        riverPersistence = r.read_f32();
        riverLacunarity = r.read_f32();
        riverWidth = r.read_f32();
        riverBankFeather = r.read_f32();
        riverDepth = r.read_f32();
        riverWarpFrequency = r.read_f32();
        riverWarpStrength = r.read_f32();
        riverMinContinentalness = r.read_f32();
        riverMaxContinentalness = r.read_f32();
        lakeFrequency = r.read_f32();
        lakeOctaves = r.read_i32();
        lakePersistence = r.read_f32();
        lakeLacunarity = r.read_f32();
        lakeThreshold = r.read_f32();
        lakeFeather = r.read_f32();
        lakeDepth = r.read_f32();
        lakeMinContinentalness = r.read_f32();
        lakeMaxContinentalness = r.read_f32();
        genSize = r.read_i32();
        downsample = r.read_i32();
        continentalnessFrequency = r.read_f32();
        continentalnessOctaves = r.read_i32();
        continentalnessPersistence = r.read_f32();
        continentalnessLacunarity = r.read_f32();
        continentalnessScalingFactor = r.read_f32();
        erosionFrequency = r.read_f32();
        erosionOctaves = r.read_i32();
        erosionPersistence = r.read_f32();
        erosionLacunarity = r.read_f32();
        erosionScalingFactor = r.read_f32();
        peakValleyFrequency = r.read_f32();
        peakValleyOctaves = r.read_i32();
        peakValleyPersistence = r.read_f32();
        peakValleyLacunarity = r.read_f32();
        peakValleyScalingFactor = r.read_f32();
        temperatureFrequency = r.read_f32();
        temperatureOctaves = r.read_i32();
        temperaturePersistence = r.read_f32();
        temperatureLacunarity = r.read_f32();
        temperatureScalingFactor = r.read_f32();
        humidityFrequency = r.read_f32();
        humidityOctaves = r.read_i32();
        humidityPersistence = r.read_f32();
        humidityLacunarity = r.read_f32();
        humidityScalingFactor = r.read_f32();
        biomeScaleChunks = r.read_i32();
        snapClimateToCells = r.read_u8() != 0;
        climateWarpFrequency = r.read_f32();
        climateWarpStrength = r.read_f32();
        desertMoistureThreshold = r.read_f32();
        forestMoistureThreshold = r.read_f32();
        snowTemperatureThreshold = r.read_f32();
        debugOresOnly = r.read_u8() != 0;
    }
};
inline AutoRegister<NetTerrainParams> _reg_NetTerrainParams;

#endif
