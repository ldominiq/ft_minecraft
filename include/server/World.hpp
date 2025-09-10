//
// Created by lucas on 7/1/25.
//

#ifndef WORLD_HPP
#define WORLD_HPP

#include <unordered_map>
#include <cmath>
#include <algorithm>
#include <unordered_set>
#include <memory>
#include <string>

#include <mutex>
#include <future>
#include <fstream>
#include <filesystem>
#include <optional>
#include <cstring>

#include "TerrainParams.hpp"
#include "Protocol.hpp"
#include "PlayerInfo.hpp"
#include "CommonWorld.hpp"

static constexpr int MAXIMUM_NUMBER_OF_CHUNKS_SENT_PER_TICK = 40;
static constexpr int REGION_SIZE = 32;

struct RegionFileMetadata {
    char magic[4] = {'R','G','N','1'};
    std::uint32_t version = 1;
    std::uint32_t regionSize = REGION_SIZE;
};

struct ChunkEntry {
	std::uint32_t X;
	std::uint32_t Z;
    std::uint32_t offset;
    std::uint32_t size;
};

class World final : public CommonWorld<ChunkGeneration>
{
    TerrainGenerationParams terrainParams;

	inline int floorDiv(int value, int divisor) {
		if (value >= 0) return value / divisor;
		return (value - divisor + 1) / divisor; // floor division for negatives
	}

	std::unordered_set<ChunkPos> plannedChunks;

    // Pending futures representing asynchronous chunk generation tasks.
    std::vector<std::future<std::pair<ChunkPos, std::shared_ptr<ChunkGeneration>>>> generationFutures;

    mutable std::mutex chunkMutex;
	bool outOfMemory = false;

    // Maximum number of chunk generation tasks that can be running at the
    // same time.  Limiting concurrency prevents CPU oversubscription and
    // reduces frame drops when many chunks need to be generated.  This
    // value can be tuned based on the number of available CPU cores.
    std::size_t maxConcurrentGeneration = 4;

	void handleOutOfMemory(int currentChunkX, int currentChunkZ, int loadRadius);
	void removeLoadedChunksFromPlayer(CPlayerInfo &player);

	std::unordered_set<ChunkPos> loadedRegions;
	void updateRegionStreaming(int currentChunkX, int currentChunkZ);
	void saveRegion(int regionX, int regionZ);
	void loadRegion(int regionX, int regionZ);
	std::string getRegionFilename(int regionX, int regionZ) const;
	std::string regionDirName;
	
public:
	World();
	World(int seed);

    ~World();

	int amountOfChunksSentThisTick = 0;
	std::vector<std::weak_ptr<ChunkGeneration>> getRenderedChunks();

    void dumpHeightmap(int centerChunkX, int centerChunkZ, int chunksX, int chunksZ, int downsample, int image) const;
    void dumpBiomeMap(int centerChunkX, int centerChunkZ, int chunksX, int chunksZ, int downsample);

    void updateVisibleChunks(CPlayerInfo &player);

    // Get or set the maximum number of chunk generation tasks that can run
    // simultaneously.  Lower values reduce CPU spikes at the cost of slower
    // world loading.  Must be at least 1.
    std::size_t getMaxConcurrentGeneration() const { return maxConcurrentGeneration; }
    void setMaxConcurrentGeneration(std::size_t n) { maxConcurrentGeneration = std::max<std::size_t>(1, n); }

	void saveRegionsOnExit();
    // Terrain params for ImGui
    TerrainGenerationParams& getTerrainParams() { return terrainParams;}

	void setCandidates(std::vector<std::tuple<int, int, float, float>> &candidates, const CPlayerInfo &player);
	void updatePlannedChunks(CPlayerInfo &player);

	std::vector<std::pair<glm::ivec3, BlockType>> updatedBlocks;

	void processPlayerMouseInputs(const CPlayerInfo &player, const NetPlayerMouseInputs &pkt);
	
	void setBlockWorld(glm::ivec3 globalCoords, std::optional<glm::ivec3> faceNormal, BlockType type) override;
};

#endif //WORLD_HPP