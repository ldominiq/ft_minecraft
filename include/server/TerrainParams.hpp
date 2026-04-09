//
// Created by lucas on 8/6/25.
//
// Server-side terrain parameters (includes shared params + server-specific data)

#ifndef SERVER_TERRAIN_PARAMS_HPP
#define SERVER_TERRAIN_PARAMS_HPP

#include "../shared/TerrainParams.hpp"
#include "Item.hpp"

struct OreParams {
    BlockType type;
    int minY;           // Minimum Y level for ore generation
    int maxY;           // Maximum Y level for ore generation
    int veinSize;       // Number of blocks in a single vein
    int veinsPerChunk;  // Number of veins per chunk
};

inline constexpr std::array<OreParams, 4> oreTable = {{
    { BlockType::IRON,      5, 64, 9, 20 },
    { BlockType::GOLD,      3, 32, 9, 2 },
    { BlockType::DIAMOND,   1, 16, 8, 1 },
    { BlockType::URANIUM,   1, 16, 4, 1 },
}};

#endif // SERVER_TERRAIN_PARAMS_HPP
