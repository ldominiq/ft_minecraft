//
// Created by lucas on 7/1/25.
//

#ifndef WORLD_HPP
#define WORLD_HPP

#include "Chunk.hpp"
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

class World {

    TerrainGenerationParams terrainParams;

	inline int floorDiv(int value, int divisor) {
		if (value >= 0) return value / divisor;
		return (value - divisor + 1) / divisor; // floor division for negatives
	}

    std::unordered_map<ChunkPos, std::shared_ptr<Chunk>> chunks;
	std::unordered_set<ChunkPos> plannedChunks;

    // Pending futures representing asynchronous chunk generation tasks.
    std::vector<std::future<std::pair<ChunkPos, std::shared_ptr<Chunk>>>> generationFutures;

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

    void dumpHeightmap(int centerChunkX, int centerChunkZ, int chunksX, int chunksZ, int downsample, int image) const;
    void dumpBiomeMap(int centerChunkX, int centerChunkZ, int chunksX, int chunksZ, int downsample);

	std::vector<std::weak_ptr<Chunk>> getRenderedChunks();

    void updateVisibleChunks(CPlayerInfo &player);

    // Return the total number of chunks currently loaded in the world.
    std::size_t getTotalChunkCount() const;

    // Get or set the maximum number of chunk generation tasks that can run
    // simultaneously.  Lower values reduce CPU spikes at the cost of slower
    // world loading.  Must be at least 1.
    std::size_t getMaxConcurrentGeneration() const { return maxConcurrentGeneration; }
    void setMaxConcurrentGeneration(std::size_t n) { maxConcurrentGeneration = std::max<std::size_t>(1, n); }

	void globalCoordsToLocalCoords(int &x, int &y, int &z, int globalX, int globalY, int globalZ, int &chunkX, int &chunkZ);
    std::shared_ptr<Chunk> getChunk(int chunkX, int chunkZ);
	BlockType getBlockWorld(glm::ivec3 globalCoords); //unused for now
	void setBlockWorld(glm::ivec3 globalCoords, std::optional<glm::ivec3> faceNormal, BlockType type);
	bool isBlockVisibleWorld(glm::ivec3 globalCoords);

	void saveRegionsOnExit();
    // Terrain params for ImGui
    TerrainGenerationParams& getTerrainParams() { return terrainParams;}

	void setCandidates(std::vector<std::tuple<int, int, float, float>> &candidates, const CPlayerInfo &player);
	void updatePlannedChunks(CPlayerInfo &player);

	std::vector<std::pair<glm::ivec3, BlockType>> updatedBlocks;
	bool getTargetedBlock(const CPlayerInfo &player, glm::ivec3& hitBlock, glm::ivec3& faceNormal, float maxDistance = 100);
	void removeTargettedBlock(const CPlayerInfo &player);
	void setTargettedBlock(const CPlayerInfo &player);
	void processPlayerMouseInputs(const CPlayerInfo &player, const NetPlayerMouseInputs &pkt);
};

#endif //WORLD_HPP