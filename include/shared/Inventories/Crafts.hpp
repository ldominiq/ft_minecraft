
#ifndef CRAFTS_HPP
#define CRAFTS_HPP

#include "Item.hpp"

#include <array>
#include <unordered_map>

using Grid = std::array<ItemType, 9>;

//not sure if needed
struct GridHash {
    std::size_t operator()(const Grid& g) const {
        std::size_t seed = 0;
        for (const auto& cell : g) {
            seed ^= std::hash<ItemType>{}(cell)
                  + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

constexpr std::array<std::pair<Grid, ItemType>, 1> orderedRecipes = {{
    {{
        BlockType::STONE, BlockType::STONE, BlockType::STONE,
        BlockType::STONE, BlockType::STONE, BlockType::STONE,
        BlockType::STONE, BlockType::STONE, BlockType::STONE
    }, BlockType::STONE}
}};

constexpr std::array<std::pair<Grid, ItemType>, 1> unorderedRecipes = {{
	{{
		BlockType::STONE
	}, BlockType::GRASS}
}};

std::unordered_map<Grid, ItemType, GridHash> recipes;

static const std::unordered_map<Grid, ItemType, GridHash> recipes = [] {
    std::unordered_map<Grid, ItemType, GridHash> m;

    for (const auto& r : orderedRecipes)
        m[r.first] = r.second;

    return m;
}();

#endif
