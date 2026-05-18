
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
constexpr std::array<std::pair<Grid, RecipeResult>, 15> orderedRecipes = {{
	{{
		BlockType::STONE, BlockType::STONE, BlockType::STONE,
		BlockType::STONE, BlockType::STONE, BlockType::STONE,
		BlockType::STONE, BlockType::STONE, BlockType::STONE
	}, {BlockType::STONE, 9}},

	{{
		BlockType::BEGIN, BlockType::OAK_PLANKS, BlockType::BEGIN,
		BlockType::BEGIN, BlockType::OAK_PLANKS, BlockType::BEGIN,
		BlockType::BEGIN, MiscType::STICK, BlockType::BEGIN
	}, {WeaponType::WOODEN_SWORD, 1}},
	{{
		BlockType::BEGIN, BlockType::STONE, BlockType::BEGIN,
		BlockType::BEGIN, BlockType::STONE, BlockType::BEGIN,
		BlockType::BEGIN, MiscType::STICK, BlockType::BEGIN
	}, {WeaponType::STONE_SWORD, 1}},
	{{
		BlockType::BEGIN, MiscType::IRON_INGOT, BlockType::BEGIN,
		BlockType::BEGIN, MiscType::IRON_INGOT, BlockType::BEGIN,
		BlockType::BEGIN, MiscType::STICK, BlockType::BEGIN
	}, {WeaponType::IRON_SWORD, 1}},
	{{
		BlockType::BEGIN, MiscType::GOLD_INGOT, BlockType::BEGIN,
		BlockType::BEGIN, MiscType::GOLD_INGOT, BlockType::BEGIN,
		BlockType::BEGIN, MiscType::STICK, BlockType::BEGIN
	}, {WeaponType::GOLDEN_SWORD, 1}},
	{{
		BlockType::BEGIN, MiscType::DIAMOND, BlockType::BEGIN,
		BlockType::BEGIN, MiscType::DIAMOND, BlockType::BEGIN,
		BlockType::BEGIN, MiscType::STICK, BlockType::BEGIN
	}, {WeaponType::DIAMOND_SWORD, 1}},

	{{
		MiscType::IRON_INGOT, MiscType::IRON_INGOT, MiscType::IRON_INGOT,
		MiscType::IRON_INGOT, MiscType::IRON_INGOT, MiscType::IRON_INGOT,
		MiscType::IRON_INGOT, MiscType::IRON_INGOT, MiscType::IRON_INGOT
	}, {BlockType::IRON_BLOCK, 1}},
	{{
		MiscType::GOLD_INGOT, MiscType::GOLD_INGOT, MiscType::GOLD_INGOT,
		MiscType::GOLD_INGOT, MiscType::GOLD_INGOT, MiscType::GOLD_INGOT,
		MiscType::GOLD_INGOT, MiscType::GOLD_INGOT, MiscType::GOLD_INGOT
	}, {BlockType::GOLD_BLOCK, 1}},
	{{
		MiscType::DIAMOND, MiscType::DIAMOND, MiscType::DIAMOND,
		MiscType::DIAMOND, MiscType::DIAMOND, MiscType::DIAMOND,
		MiscType::DIAMOND, MiscType::DIAMOND, MiscType::DIAMOND
	}, {BlockType::DIAMOND_BLOCK, 1}},
	{{
		MiscType::URANIUM_INGOT, MiscType::URANIUM_INGOT, MiscType::URANIUM_INGOT,
		MiscType::URANIUM_INGOT, MiscType::URANIUM_INGOT, MiscType::URANIUM_INGOT,
		MiscType::URANIUM_INGOT, MiscType::URANIUM_INGOT, MiscType::URANIUM_INGOT
	}, {BlockType::URANIUM_BLOCK, 1}},

	{{
		MiscType::IRON_INGOT, BlockType::BEGIN, MiscType::IRON_INGOT,
		BlockType::BEGIN, MiscType::IRON_INGOT, BlockType::BEGIN,
		BlockType::BEGIN, BlockType::BEGIN, BlockType::BEGIN
	}, {MiscType::EMPTY_BUCKET, 1}},

	{{
		BlockType::BEGIN, BlockType::BEGIN, BlockType::BEGIN,
		MiscType::IRON_INGOT, BlockType::BEGIN, MiscType::IRON_INGOT,
		BlockType::BEGIN, MiscType::IRON_INGOT, BlockType::BEGIN
	}, {MiscType::EMPTY_BUCKET, 1}},

	{{
		MiscType::COAL, MiscType::COAL, MiscType::COAL,
		MiscType::COAL, MiscType::COAL, MiscType::COAL,
		MiscType::COAL, MiscType::COAL, MiscType::COAL
	}, {BlockType::COAL_BLOCK, 1}},
}};

constexpr std::array<std::pair<Grid, RecipeResult>, 21> shapelessRecipes = {{
	{{
		BlockType::STONE
	}, {BlockType::GRASS, 1}},

	{{
		BlockType::IRON
	}, {MiscType::IRON_INGOT, 1}},
	{{
		BlockType::GOLD
	}, {MiscType::GOLD_INGOT, 1}},
	{{
		BlockType::DIAMOND
	}, {MiscType::DIAMOND, 1}},
	{{
		BlockType::URANIUM
	}, {MiscType::URANIUM_INGOT, 1}},

	{{
		BlockType::OAK_LOG
	}, {BlockType::OAK_PLANKS, 4}},
	{{
		BlockType::BIRCH_LOG
	}, {BlockType::OAK_PLANKS, 4}},
	{{
		BlockType::ACACIA_LOG
	}, {BlockType::OAK_PLANKS, 4}},
	{{
		BlockType::JUNGLE_LOG
	}, {BlockType::OAK_PLANKS, 4}},
	{{
		BlockType::SPRUCE_LOG
	}, {BlockType::OAK_PLANKS, 4}},
	{{
		BlockType::DARK_OAK_LOG
	}, {BlockType::OAK_PLANKS, 4}},

	{{
		BlockType::OAK_PLANKS, BlockType::OAK_PLANKS
	}, {MiscType::STICK, 4}},

	//beacon used for set spawn point.
	{{
		BlockType::IRON_BLOCK, BlockType::DIAMOND_BLOCK, BlockType::GOLD_BLOCK,
		BlockType::URANIUM_BLOCK, BlockType::STONE, BlockType::WITHER_ROSE,
		BlockType::SAND, BlockType::NETHERRACK, BlockType::SNOW
	}, {BlockType::BEACON, 1}},

	{{
	   MiscType::COAL, MiscType::STICK
	}, {BlockType::TORCH_FLOOR, 4}},

	{{
	   BlockType::COAL
	}, {MiscType::COAL, 1}},

	{{
	   BlockType::IRON_BLOCK
	}, {MiscType::IRON_INGOT, 9}},

	{{
	   BlockType::GOLD_BLOCK
	}, {MiscType::GOLD_INGOT, 9}},

	{{
	   BlockType::DIAMOND_BLOCK
	}, {MiscType::DIAMOND, 9}},
	{{
	   BlockType::URANIUM_BLOCK
	}, {MiscType::URANIUM_INGOT, 9}},

	{{
	   BlockType::COAL_BLOCK
	}, {MiscType::COAL, 9}},

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
