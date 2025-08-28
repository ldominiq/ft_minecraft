
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

#include "Block.hpp"
#include "BitPackedArray.hpp"

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
        return h1 ^ (h2 << 1);
    }
};

struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        return std::hash<int>()(v.x) ^ std::hash<int>()(v.y << 1) ^ std::hash<int>()(v.z << 2);
    }
};

class Chunk {

	protected:
		int originX; // X coordinate of the chunck origin
		int originZ; // Z coordinate of the chunck origin

		std::vector<BlockType> palette; // Index -> BlockType
		std::unordered_map<BlockType, uint32_t> paletteMap; // BlockType -> Index
		BitPackedArray blockIndices;
		
		std::weak_ptr<Chunk> adjacentChunks[4] = {};

	public:

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

		BlockType getBlock(int x, int y, int z) const;
		void setBlock(int x, int y, int z, BlockType block);

		bool isBlockVisible(glm::ivec3 blockPos);

		void saveToStream(std::ostream& out) const; // only server? Still great to have it here.
		void loadFromStream(std::istream& in);

		inline const std::weak_ptr<Chunk>(&getAdjacentChunks() const)[4] { return adjacentChunks;}

		static inline ChunkPos toKey(int32_t chunkX, int32_t chunkZ) { //boff
			return std::make_pair(chunkX, chunkZ);
		}
};

inline Chunk::~Chunk() {}

#endif