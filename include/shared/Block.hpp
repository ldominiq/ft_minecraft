#ifndef BLOCK_HPP
#define BLOCK_HPP
#include <cstdint>

enum class BlockType : uint8_t{
    AIR,
    GRASS,
    DIRT,
    STONE,
    SAND,
    SNOW,
    WATER,
    BEDROCK,
    LOG,
    LEAVES
};

enum class BiomeType {
    PLAINS,
    DESERT,
    FOREST,
    TUNDRA,
	SWAMP,
	OCEAN,
	MOUNTAIN
};

struct Voxel {
    BlockType type;
    uint8_t skyLight; // 0-15, sunlight propagated from sky
    uint8_t blockLight; // 0-15, emitted from torches, etc.
};

class Block {
public:
    explicit Block(BlockType type = BlockType::AIR);
    BlockType getType() const;
    bool isVisible() const;
private:
    BlockType type;
};

#endif