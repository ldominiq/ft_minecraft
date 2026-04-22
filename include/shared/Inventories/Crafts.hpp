
#ifndef CRAFTS_HPP
#define CRAFTS_HPP

#include "Item.hpp"

#include <array>
#include <unordered_map>

using Grid = std::array<ItemType, 9>;

struct GridHash {
    std::size_t operator()(const Grid& g) const {
        std::size_t seed = 0;
        for (auto item : g) {
            seed ^= std::hash<ItemID>{}(itemTypeToItemID(item))
                  + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

using Key = std::array<uint8_t, (ItemID)MiscType::END>;
struct KeyHash {
    std::size_t operator()(const Key& k) const {
        std::size_t seed = 0;
        for (auto count : k) {
            seed ^= std::hash<uint8_t>{}(count)
                  + 0x9e3779b9 + (seed << 6) + (seed >> 2);
        }
        return seed;
    }
};

struct RecipeResult {
	ItemType type;
	int count;
};

// Here we define the recipes.
constexpr std::array<std::pair<Grid, RecipeResult>, 1> orderedRecipes = {{
	{{
		BlockType::STONE, BlockType::STONE, BlockType::STONE,
		BlockType::STONE, BlockType::STONE, BlockType::STONE,
		BlockType::STONE, BlockType::STONE, BlockType::STONE
	}, {BlockType::STONE, 1}}
}};

constexpr std::array<std::pair<Grid, RecipeResult>, 1> shapelessRecipes = {{
	{{
		BlockType::STONE
	}, {BlockType::GRASS, 1}}
}};

//this wastes a bit of space but it's blazingly faaast. maybe.
//Saves an array that has (ItemID)MiscType::END) amount of elements.
//If an element in the recipe is present we increment the index of the corresponding item.
static Key gridToKey(const Grid& grid) {
    Key k{};
    for (auto item : grid) {
        if (item != ItemType{}) // skip empty slots
            k[itemTypeToItemID(item)]++;
    }
    return k;
}

template <size_t N>
static std::array<std::pair<Key, RecipeResult>, N>
makeShapelessKeys(const std::array<std::pair<Grid, RecipeResult>, N>& recipes) {
    std::array<std::pair<Key, RecipeResult>, N> result{};
    for (size_t i = 0; i < N; ++i) {
        result[i] = {gridToKey(recipes[i].first), recipes[i].second};
    }
    return result;
}

struct Recipes {
    inline static const auto shapelessRecipesKeys = makeShapelessKeys(shapelessRecipes);
    inline static const auto shapelessRecipeMap = [] {
        std::unordered_map<Key, RecipeResult, KeyHash> m;
        for (auto &r : shapelessRecipesKeys)
            m[r.first] = r.second;
        return m;
    }();
    inline static const auto orderedRecipeMap = [] {
        std::unordered_map<Grid, RecipeResult, GridHash> m;
        for (auto &r : orderedRecipes)
            m[r.first] = r.second;
        return m;
    }();
};



#endif
