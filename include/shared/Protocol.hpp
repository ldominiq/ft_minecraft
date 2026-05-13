#ifndef PROTOCOL_HPP
#define PROTOCOL_HPP

#include <iostream>

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

	// Utility: decode inner packets. A single malformed inner packet must not
	// kill the whole group (or the process), so failures are logged and
	// skipped; callers only see the packets that decoded successfully.
	std::vector<PacketPtr> unpack() const {
		std::vector<PacketPtr> result;
		result.reserve(rawPackets.size());
		for (auto& raw : rawPackets) {
			try {
				auto pkt = decodePacket(raw.data(), raw.size());
				if (pkt) result.push_back(std::move(pkt));
			} catch (const std::exception& e) {
				std::cerr << "[Network] group inner decode failed: " << e.what() << "\n";
			}
		}
		return result;
	}
};
inline AutoRegister<NetPacketGroup> _reg_NetPacketGroup;

struct NetConnect final : public Packet {
    static constexpr PacketType ID = PacketType::NET_CONNECT;
    std::string username;

    NetConnect() : Packet(ID) { flags = PacketFlags::Reliable; }

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

    NetDisconnect() : Packet(ID) { flags = PacketFlags::Reliable; }

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
    uint32_t clientId     = 0;
    uint32_t playerListId = 0;

    NetAccept() : Packet(ID) { flags = PacketFlags::Reliable; }

    void encode(BufferWriter& w) const override { w.write_u32(clientId); w.write_u32(playerListId); }
    void decode(BufferReader& r) override { clientId = r.read_u32(); playerListId = r.read_u32(); }
};
inline AutoRegister<NetAccept> _reg_NetAccept;

struct NetSetName final : public Packet {
	static constexpr PacketType ID = PacketType::NET_SET_NAME;
	std::string username;

	NetSetName() : Packet(ID) { flags = PacketFlags::Reliable; }

	void encode(BufferWriter& w) const override {
		w.write_string(username);
	}
	void decode(BufferReader& r) override {
		username = r.read_string();
	}
};
inline AutoRegister<NetSetName> _reg_NetSetName;

struct NetPlayerInputs final : public Packet {
    static constexpr PacketType ID = PacketType::PLAYER_INPUT;

	int32_t serverClientReconciliationTick = -1; //for client reconciliation.

	uint16_t keys = 0;	// bitfield
	uint8_t activeHotbarSlot = -1;
    float pitch = 0.0f;   // absolute rotation around X axis
    float yaw = 0.0f;     // absolute rotation around Y axis
	uint8_t loadRadius = 4; // maybe this should go elsewhere. Oh well!

    NetPlayerInputs() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
		w.write_i32(serverClientReconciliationTick);
        w.write_u16(keys);
		w.write_u8(activeHotbarSlot);
        w.write_f32(pitch);
        w.write_f32(yaw);
		w.write_u8(loadRadius);
    }

    void decode(BufferReader& r) override {
        serverClientReconciliationTick = r.read_i32();
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

// TODO : add delta compression & put inside of a new Snapshot packet sometimeTM
struct NetPlayerMove final : public Packet {
	static constexpr PacketType ID = PacketType::PLAYER_MOVE;
	int32_t serverClientReconciliationTick = 0; //server tick at which the client has done his inputs/prediction corresponding to this packet.

	// Player position is sent in double precision so that, far from world
	// origin, the wire value doesn't snap to the f32 grid (~0.06 units at
	// x=1e6) — which would re-introduce visible jitter every server tick.
	double positionX;
	double positionY;
	double positionZ;

	float velocityX;
	float velocityY;
	float velocityZ;

	float yaw;
	float pitch;

	float health;
	float slipperinessPrev;
	float accumulatedFallDistance;
	uint8_t onGround = 0;
	uint8_t jumpBoostApplied = 0;

	NetPlayerMove() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
		w.write_i32(serverClientReconciliationTick);
		w.write_f64(positionX);
		w.write_f64(positionY);
		w.write_f64(positionZ);
		w.write_f32(velocityX);
		w.write_f32(velocityY);
		w.write_f32(velocityZ);
		w.write_f32(yaw);
		w.write_f32(pitch);
		w.write_f32(health);
       w.write_f32(slipperinessPrev);
		w.write_f32(accumulatedFallDistance);
		w.write_u8(onGround);
		w.write_u8(jumpBoostApplied);
    }
    void decode(BufferReader& r) override {
		serverClientReconciliationTick = r.read_i32();
		positionX = r.read_f64();
		positionY = r.read_f64();
		positionZ = r.read_f64();
		velocityX = r.read_f32();
		velocityY = r.read_f32();
		velocityZ = r.read_f32();
		yaw = r.read_f32();
		pitch = r.read_f32();
		health = r.read_f32();
     slipperinessPrev = r.read_f32();
		accumulatedFallDistance = r.read_f32();
		onGround = r.read_u8();
		jumpBoostApplied = r.read_u8();
    }
};
inline AutoRegister<NetPlayerMove> _reg_NetPlayerMove;

struct NetPlayerGameMode final : public Packet {
	static constexpr PacketType ID = PacketType::PLAYER_GAMEMODE;
	uint8_t gamemode = 0; // 0 = survival, 1 = creative, 2 = adventure, 3 = spectator

	NetPlayerGameMode() : Packet(ID) { flags = PacketFlags::Reliable; }

    void encode(BufferWriter& w) const override {
		w.write_u8(gamemode);
    }
    void decode(BufferReader& r) override {
		gamemode = r.read_u8();
    }
};
inline AutoRegister<NetPlayerGameMode> _reg_NetPlayerGameMode;


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

	// Position is sent in double precision. At ~5M blocks from origin, f32 ULP is
	// ~0.5 blocks — diagonal walking quantizes onto a coarser grid for X vs Z and
	// produces visible zig-zag on the receiving client (sender's own position is
	// fine because it predicts locally). dvec3 keeps sub-block precision past 1e8.
	double positionX;
	double positionY;
	double positionZ;

	float yaw;
	float pitch = 0.0f;

	// bit 0 = hasHorizontalInput, bit 1 = onGround, bit 2 = armSwing event,
	// bit 3 = primed (creeper fuse), bit 4 = hurt event (entity took damage this tick),
	// bit 5 = diedByExplosion (only meaningful when type==-1, i.e. the death packet)
	std::string entityName = ""; //should go to a separate packet send on NetAccept to be sent only once and not take bandwidth every tick.
	uint8_t positionFlags = 0;

	NetEntityMove() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
		w.write_u8(eEntityType);
		w.write_u32(entityID);
		w.write_u16(type);
		w.write_f64(positionX);
		w.write_f64(positionY);
		w.write_f64(positionZ);
		w.write_f32(yaw);
		w.write_f32(pitch);
		w.write_u8(positionFlags);
		w.write_string(entityName);
    }
    void decode(BufferReader& r) override {
		eEntityType = static_cast<EEntityTypes>(r.read_u8());
		entityID = r.read_u32();
		type = r.read_u16();
		positionX = r.read_f64();
		positionY = r.read_f64();
		positionZ = r.read_f64();
		yaw = r.read_f32();
		pitch = r.read_f32();
		positionFlags = r.read_u8();
		entityName = r.read_string();
    }
};
inline AutoRegister<NetEntityMove> _reg_NetEntityMove;

struct NetInventory final : public Packet {
	static constexpr PacketType ID = PacketType::NET_INVENTORY;

	uint8_t inventoryTypeID = 0;

	uint16_t type = 0;
	uint8_t amount = 0;
	uint8_t slot = 0;	// HAND_ID for hand (37)

	NetInventory() : Packet(ID) { flags = PacketFlags::Reliable; }

	void encode(BufferWriter& w) const override {
		w.write_u8(inventoryTypeID);
		w.write_u16(type);
		w.write_u8(amount);
		w.write_u8(slot);
    }

	void decode(BufferReader& r) override {
		inventoryTypeID = r.read_u8();
		type = r.read_u16();
		amount = r.read_u8();
		slot = r.read_u8();
    }
};
inline AutoRegister<NetInventory> _reg_NetInventory;

struct NetInventoryAction final : public Packet {
    static constexpr PacketType ID = PacketType::NET_INVENTORY_ACTION;

	uint8_t inventoryTypeID = 0;

	uint8_t actionType = -1; // right click, left click
	uint8_t modifier = -1;	// shift, ctrl, alt, drag, drop, etc...
	uint8_t slot = -1;

    NetInventoryAction() : Packet(ID) { flags = PacketFlags::Reliable; }

    void encode(BufferWriter& w) const override {
		w.write_u8(inventoryTypeID);
        w.write_u8(actionType);
        w.write_u8(modifier);
        w.write_u8(slot);
    }

    void decode(BufferReader& r) override {
        inventoryTypeID = r.read_u8();
        actionType = r.read_u8();
        modifier = r.read_u8();
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

    NetChunkHeader() : Packet(ID) { flags = PacketFlags::Reliable; }

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

    NetChunkData() : Packet(ID) { flags = PacketFlags::Reliable; }

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
	
	NetModifiedBlockData() : Packet(ID) { flags = PacketFlags::Reliable; }

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
	
	NetMessage() : Packet(ID) { flags = PacketFlags::Reliable; }

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
	uint8_t contBucket = 0;
	uint8_t erosionBucket = 0;
	uint8_t pvBucket = 0;
	uint8_t tempBucket = 0;
	uint8_t humidBucket = 0;

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
        w.write_u8(contBucket);
        w.write_u8(erosionBucket);
        w.write_u8(pvBucket);
        w.write_u8(tempBucket);
        w.write_u8(humidBucket);
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
        contBucket = r.read_u8();
        erosionBucket = r.read_u8();
        pvBucket = r.read_u8();
        tempBucket = r.read_u8();
        humidBucket = r.read_u8();
    }
};
inline AutoRegister<NetImGui> _reg_NetImGui;

struct NetSkyTime : public Packet {
	static constexpr PacketType ID = PacketType::NET_SKY_TIME;
	float   skyTimeOffset  = 0.0f;
	float   sunYawDeg      = 0.0f;
	bool    skyTimePaused  = false;
	bool    sunStepping    = false;
	float   sunPauseTimer  = 0.0f;
	float   sunStepTimer   = 0.0f;
	uint8_t skyMode        = 0;     // 0 = Skyrim (pause/step), 1 = Smooth (linear)
	float   skyTimeSpeed   = 0.05f; // sun advancement speed multiplier

	NetSkyTime() : Packet(ID) {}

	void encode(BufferWriter& w) const override {
		w.write_f32(skyTimeOffset);
		w.write_f32(sunYawDeg);
		w.write_u8(skyTimePaused ? 1 : 0);
		w.write_u8(sunStepping  ? 1 : 0);
		w.write_f32(sunPauseTimer);
		w.write_f32(sunStepTimer);
		w.write_u8(skyMode);
		w.write_f32(skyTimeSpeed);
	}

	void decode(BufferReader& r) override {
		skyTimeOffset = r.read_f32();
		sunYawDeg     = r.read_f32();
		skyTimePaused = r.read_u8() != 0;
		sunStepping   = r.read_u8() != 0;
		sunPauseTimer = r.read_f32();
		sunStepTimer  = r.read_f32();
		skyMode       = r.read_u8();
		skyTimeSpeed  = r.read_f32();
	}
};
inline AutoRegister<NetSkyTime> _reg_NetSkyTime;
// Terrain generation parameters sync packet (C2S && S2C)
struct NetTerrainParams final : public Packet {
    static constexpr PacketType ID = PacketType::NET_TERRAIN_PARAMS;

    // All defaults are derived from TerrainGenerationParams to stay in sync.
    // If a field is added, also update toParams() and fromParams() below...
    int32_t seed;
    int32_t seaLevel;
    int32_t bedrockLevel;

    // River carving params
    float riverFrequency;
    int32_t riverOctaves;
    float riverPersistence;
    float riverLacunarity;
    float riverWidth;
    float riverBankFeather;
    float riverDepth;
    float riverWarpFrequency;
    float riverWarpStrength;
    float riverMinContinentalness;
    float riverMaxContinentalness;

    // Lake carving params
    float lakeFrequency;
    int32_t lakeOctaves;
    float lakePersistence;
    float lakeLacunarity;
    float lakeThreshold;
    float lakeFeather;
    float lakeDepth;
    float lakeMinContinentalness;
    float lakeMaxContinentalness;

    // Heightmap dump settings
    int32_t genSize = 1000;
    int32_t downsample = 16;

    // Continentalness noise params
    float continentalnessFrequency;
    int32_t continentalnessOctaves;
    float continentalnessPersistence;
    float continentalnessLacunarity;
    float continentalnessScalingFactor;

    // Erosion noise params
    float erosionFrequency;
    int32_t erosionOctaves;
    float erosionPersistence;
    float erosionLacunarity;
    float erosionScalingFactor;

    // Peak / valley noise params
    float peakValleyFrequency;
    int32_t peakValleyOctaves;
    float peakValleyPersistence;
    float peakValleyLacunarity;
    float peakValleyScalingFactor;

    // Temperature noise params
    float temperatureFrequency;
    int32_t temperatureOctaves;
    float temperaturePersistence;
    float temperatureLacunarity;
    float temperatureScalingFactor;

    // Humidity noise params
    float humidityFrequency;
    int32_t humidityOctaves;
    float humidityPersistence;
    float humidityLacunarity;
    float humidityScalingFactor;

    // Biome params
    int32_t biomeScaleChunks;
    bool snapClimateToCells;
    float climateWarpFrequency;
    float climateWarpStrength;

    bool debugOresOnly;

    TerrainGenerationParams toParams() const {
        TerrainGenerationParams p;
        p.seed = seed; p.seaLevel = seaLevel; p.bedrockLevel = bedrockLevel;
        p.riverFrequency = riverFrequency; p.riverOctaves = riverOctaves;
        p.riverPersistence = riverPersistence; p.riverLacunarity = riverLacunarity;
        p.riverWidth = riverWidth; p.riverBankFeather = riverBankFeather;
        p.riverDepth = riverDepth; p.riverWarpFrequency = riverWarpFrequency;
        p.riverWarpStrength = riverWarpStrength;
        p.riverMinContinentalness = riverMinContinentalness;
        p.riverMaxContinentalness = riverMaxContinentalness;
        p.lakeFrequency = lakeFrequency; p.lakeOctaves = lakeOctaves;
        p.lakePersistence = lakePersistence; p.lakeLacunarity = lakeLacunarity;
        p.lakeThreshold = lakeThreshold; p.lakeFeather = lakeFeather;
        p.lakeDepth = lakeDepth;
        p.lakeMinContinentalness = lakeMinContinentalness;
        p.lakeMaxContinentalness = lakeMaxContinentalness;
        p.genSize = genSize; p.downsample = downsample;
        p.continentalnessFrequency = continentalnessFrequency;
        p.continentalnessOctaves = continentalnessOctaves;
        p.continentalnessPersistence = continentalnessPersistence;
        p.continentalnessLacunarity = continentalnessLacunarity;
        p.continentalnessScalingFactor = continentalnessScalingFactor;
        p.erosionFrequency = erosionFrequency; p.erosionOctaves = erosionOctaves;
        p.erosionPersistence = erosionPersistence; p.erosionLacunarity = erosionLacunarity;
        p.erosionScalingFactor = erosionScalingFactor;
        p.peakValleyFrequency = peakValleyFrequency; p.peakValleyOctaves = peakValleyOctaves;
        p.peakValleyPersistence = peakValleyPersistence; p.peakValleyLacunarity = peakValleyLacunarity;
        p.peakValleyScalingFactor = peakValleyScalingFactor;
        p.temperatureFrequency = temperatureFrequency; p.temperatureOctaves = temperatureOctaves;
        p.temperaturePersistence = temperaturePersistence; p.temperatureLacunarity = temperatureLacunarity;
        p.temperatureScalingFactor = temperatureScalingFactor;
        p.humidityFrequency = humidityFrequency; p.humidityOctaves = humidityOctaves;
        p.humidityPersistence = humidityPersistence; p.humidityLacunarity = humidityLacunarity;
        p.humidityScalingFactor = humidityScalingFactor;
        p.biomeScaleChunks = biomeScaleChunks; p.snapClimateToCells = snapClimateToCells;
        p.climateWarpFrequency = climateWarpFrequency; p.climateWarpStrength = climateWarpStrength;
        p.debugOresOnly = debugOresOnly;
        return p;
    }

    static NetTerrainParams fromParams(const TerrainGenerationParams& p) {
        NetTerrainParams pkt(p);
        return pkt;
    }

    explicit NetTerrainParams(const TerrainGenerationParams& p) : Packet(ID) {
        flags = PacketFlags::Reliable;
        seed = p.seed; seaLevel = p.seaLevel; bedrockLevel = p.bedrockLevel;
        riverFrequency = p.riverFrequency; riverOctaves = p.riverOctaves;
        riverPersistence = p.riverPersistence; riverLacunarity = p.riverLacunarity;
        riverWidth = p.riverWidth; riverBankFeather = p.riverBankFeather;
        riverDepth = p.riverDepth; riverWarpFrequency = p.riverWarpFrequency;
        riverWarpStrength = p.riverWarpStrength;
        riverMinContinentalness = p.riverMinContinentalness;
        riverMaxContinentalness = p.riverMaxContinentalness;
        lakeFrequency = p.lakeFrequency; lakeOctaves = p.lakeOctaves;
        lakePersistence = p.lakePersistence; lakeLacunarity = p.lakeLacunarity;
        lakeThreshold = p.lakeThreshold; lakeFeather = p.lakeFeather;
        lakeDepth = p.lakeDepth;
        lakeMinContinentalness = p.lakeMinContinentalness;
        lakeMaxContinentalness = p.lakeMaxContinentalness;
        genSize = p.genSize; downsample = p.downsample;
        continentalnessFrequency = p.continentalnessFrequency;
        continentalnessOctaves = p.continentalnessOctaves;
        continentalnessPersistence = p.continentalnessPersistence;
        continentalnessLacunarity = p.continentalnessLacunarity;
        continentalnessScalingFactor = p.continentalnessScalingFactor;
        erosionFrequency = p.erosionFrequency; erosionOctaves = p.erosionOctaves;
        erosionPersistence = p.erosionPersistence; erosionLacunarity = p.erosionLacunarity;
        erosionScalingFactor = p.erosionScalingFactor;
        peakValleyFrequency = p.peakValleyFrequency; peakValleyOctaves = p.peakValleyOctaves;
        peakValleyPersistence = p.peakValleyPersistence; peakValleyLacunarity = p.peakValleyLacunarity;
        peakValleyScalingFactor = p.peakValleyScalingFactor;
        temperatureFrequency = p.temperatureFrequency; temperatureOctaves = p.temperatureOctaves;
        temperaturePersistence = p.temperaturePersistence; temperatureLacunarity = p.temperatureLacunarity;
        temperatureScalingFactor = p.temperatureScalingFactor;
        humidityFrequency = p.humidityFrequency; humidityOctaves = p.humidityOctaves;
        humidityPersistence = p.humidityPersistence; humidityLacunarity = p.humidityLacunarity;
        humidityScalingFactor = p.humidityScalingFactor;
        biomeScaleChunks = p.biomeScaleChunks; snapClimateToCells = p.snapClimateToCells;
        climateWarpFrequency = p.climateWarpFrequency; climateWarpStrength = p.climateWarpStrength;
        debugOresOnly = p.debugOresOnly;
    }

    NetTerrainParams() : NetTerrainParams(TerrainGenerationParams{}) {}

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
        debugOresOnly = r.read_u8() != 0;
    }
};
inline AutoRegister<NetTerrainParams> _reg_NetTerrainParams;

struct NetPing final : public Packet {
    static constexpr PacketType ID = PacketType::NET_PING;
	uint64_t timestamp = 0; // client timestamp when ping was sent
	NetPing() : Packet(ID) {}
    void encode(BufferWriter& w) const override {
        w.write_u64(timestamp);
    }
    void decode(BufferReader& r) override {
        timestamp = r.read_u64();
	}
};
inline AutoRegister<NetPing> _reg_NetPing;

struct NetPong final : public Packet {
	static constexpr PacketType ID = PacketType::NET_PONG;
	uint64_t timestamp = 0; // copy of client timestamp from ping
	NetPong() : Packet(ID) {}
    void encode(BufferWriter& w) const override {
        w.write_u64(timestamp);
	}
    void decode(BufferReader& r) override {
        timestamp = r.read_u64();
	}
};
inline AutoRegister<NetPong> _reg_NetPong;

// Client → Server: report measured round-trip ping
struct NetPlayerPing final : public Packet {
    static constexpr PacketType ID = PacketType::NET_PLAYER_PING;
    float pingMs = -1.0f;
    NetPlayerPing() : Packet(ID) {}
    void encode(BufferWriter& w) const override { w.write_f32(pingMs); }
    void decode(BufferReader& r) override { pingMs = r.read_f32(); }
};
inline AutoRegister<NetPlayerPing> _reg_NetPlayerPing;

// Server → all clients: list of (entityID, pingMs) for every connected player
struct NetPingList final : public Packet {
    static constexpr PacketType ID = PacketType::NET_PING_LIST;
    struct Entry { uint32_t entityId; uint32_t playerListId; float pingMs; };
    std::vector<Entry> entries;
    NetPingList() : Packet(ID) {}
    void encode(BufferWriter& w) const override {
        w.write_u8(static_cast<uint8_t>(entries.size()));
        for (const auto& e : entries) {
            w.write_u32(e.entityId);
            w.write_u32(e.playerListId);
            w.write_f32(e.pingMs);
        }
    }
    void decode(BufferReader& r) override {
        uint8_t count = r.read_u8();
        entries.resize(count);
        for (auto& e : entries) {
            e.entityId    = r.read_u32();
            e.playerListId = r.read_u32();
            e.pingMs      = r.read_f32();
        }
    }
};
inline AutoRegister<NetPingList> _reg_NetPingList;

// Receiver-driven retransmit request for the reliability layer.
// Means: "resend packets with reliableSeq in [fromSeq, toSeq] inclusive."
// Not itself flagged Reliable (meta).
struct NetReliableNack final : public Packet {
    static constexpr PacketType ID = PacketType::RELIABLE_NACK;
    uint32_t fromSeq = 0;
    uint32_t toSeq   = 0;

    NetReliableNack() : Packet(ID) {}

    void encode(BufferWriter& w) const override {
        w.write_u32(fromSeq);
        w.write_u32(toSeq);
    }
    void decode(BufferReader& r) override {
        fromSeq = r.read_u32();
        toSeq   = r.read_u32();
    }
};
inline AutoRegister<NetReliableNack> _reg_NetReliableNack;

#endif
