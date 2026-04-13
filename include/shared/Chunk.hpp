
#ifndef CHUNK_HPP
#define CHUNK_HPP

#include <vector>
#include <glm/glm.hpp>

#include <cstdlib>
#include <cmath>
#include <iostream>
#include <ostream>
#include <fstream>

#include <random>
#include <unordered_set>
#include <queue>
#include <memory>

#include "Item.hpp"
#include "BitPackedArray.hpp"
#include "LiquidsManager.hpp"

class BlockStorage;

enum Direction {
	NORTH = 0,
	SOUTH,
	EAST,
	WEST,
	NONE
};

using ChunkPos = std::pair<int32_t, int32_t>;

template <>
struct std::hash<ChunkPos> {
    std::size_t operator()(const ChunkPos& p) const noexcept {
        std::size_t h1 = std::hash<int>()(p.first);
        std::size_t h2 = std::hash<int>()(p.second);
		// original formula produced collisions in a very predictable pattern. see boost::hash_combine
		// 2654435761u large odd number - 0x9e3779b9u = 2^32 / φ so input 0 doesn't output 0
		// (h1 << 6) + (h1 >> 2) — this mixes h1 with itself at two different bit positions, so the output depends on h1 in a non-trivial way.
        return h1 ^ (h2 * 2654435761u + 0x9e3779b9u + (h1 << 6) + (h1 >> 2));
    }
};

struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        return std::hash<int>()(v.x) ^ std::hash<int>()(v.y << 1) ^ std::hash<int>()(v.z << 2);
    }
};

class Chunk {
	public:

		std::vector<s_liquid> liquids; //TODO put this in the bitpacker.

		// Vegetation: store local positions (x,y,z) and type
		// Uses local chunk coordinates (0-15 for x/z, 0-255 for y)
		struct VegetationInstance {
			uint8_t x, y, z;  // local coordinates within chunk
			BlockType type;   // SHORT_GRASS, TALL_GRASS, CORNFLOWER
			uint8_t biome;   // biome type for tinting
		};
		std::vector<VegetationInstance> vegetation;

		Chunk(int chunkX, int chunkZ, int bitsPerEntry = 4)
        : originX(chunkX * WIDTH),
          originZ(chunkZ * DEPTH),
          blockIndices(WIDTH * HEIGHT * DEPTH, bitsPerEntry) {}
		Chunk(std::istream& in);
		virtual ~Chunk() = 0; // pure virtual destructor

		static constexpr int WIDTH = 16; // Size of the chunck in blocks
		static constexpr int HEIGHT = 256; // Height of the chunck in blocks
		static constexpr int DEPTH = 16; // Depth of the chunck in blocks

		static constexpr int BLOCK_COUNT = WIDTH * HEIGHT * DEPTH;

		int getOriginX() const { return originX; }
		int getOriginZ() const { return originZ; }

		template <typename ChunkT>
		void setAdjacentChunks(int direction, std::shared_ptr<ChunkT> const& chunk) {
			static_assert(std::is_base_of_v<Chunk, ChunkT>, "T must derive from Chunk");
			adjacentChunks[direction] = chunk;
		}
		bool hasAllAdjacentChunkLoaded() const;

		BlockType getBlock(int x, int y, int z) const;
		void setBlock(int x, int y, int z, BlockType block);
		// Like setBlock, but also clears any land vegetation directly above when placing AIR.
		// Returns true if a vegetation block above was also cleared.
		bool setBlockCascade(int x, int y, int z, BlockType type);

		bool isBlockVisible(glm::ivec3 blockPos);

		// Compute sky-light for this chunk using BFS flood-fill from the top.
		// Call this after block data is loaded and before mesh building.
		void computeSkyLight();

		// Get the sky-light level at a local block position (0-15).
		// Returns 0 for out-of-bounds positions (fully dark).
		uint8_t getSkyLight(int x, int y, int z) const;

		// Returns true if sky-light has already been computed for this chunk.
		bool hasSkyLight() const { return !skyLight.empty(); }

		void saveToStream(std::ostream& out) const; // only server? Still great to have it here.
		void loadFromStream(std::istream& in);

		inline const std::weak_ptr<Chunk>(&getAdjacentChunks() const)[4] { return adjacentChunks;}
		inline ChunkPos getPos() const { return ChunkPos{originX / WIDTH, originZ / DEPTH}; }

		static inline ChunkPos toKey(int32_t chunkX, int32_t chunkZ) { //boff
			return std::make_pair(chunkX, chunkZ);
		}

		BiomeType getBiomeAt(int localX, int localZ) const;
		void setBiomeAt(int localX, int localZ, BiomeType biome);

	protected:
		int originX; // X coordinate of the chunck origin
		int originZ; // Z coordinate of the chunck origin

		std::vector<BlockType> palette; // Index -> BlockType
		std::unordered_map<BlockType, uint32_t> paletteMap; // BlockType -> Index
		BitPackedArray blockIndices;
		
		std::weak_ptr<Chunk> adjacentChunks[4] = {};

		// Sky-light level per block (0 = full darkness, 15 = full sunlight).
		// Indexed as: x + WIDTH * (y + HEIGHT * z)
		// Computed client-side during mesh building via BFS flood-fill.
		std::vector<uint8_t> skyLight;

		std::array<BiomeType, WIDTH * DEPTH> biomeMap{}; // Biome type per block
};

#endif