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
    LEAVES,
	END
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

#endif