
#ifndef ITEM_HPP
#define ITEM_HPP

#include <cstdint>
#include <string>
#include <variant>
#include <array>
#include <vector>

using ItemID  = uint16_t;  // for inventory/items

enum class BiomeType : uint8_t {
    PLAINS,
    DESERT,
    DARK_FOREST,
	JUNGLE,
	SAVANNA,
	BIRCH_FOREST,
	MESA,
    TUNDRA,
	SWAMP,
	OCEAN,
	MOUNTAIN,
	ICE_PLAINS,
	VOLCANIC,
	RED_DESERT,
	NETHER,
	MUSHROOM_ISLAND,
};

enum class ClimateTemperature : uint8_t {
	VERY_COLD = 0,
	COLD = 1,
	TEMPERATE = 2,
	WARM = 3,
	HOT = 4
};

enum class ClimateHumidity : uint8_t {
	ARID = 0,
	DRY = 1,
	NEUTRAL = 2,
	HUMID = 3,
	WET = 4
};

enum class ClimateErosion : uint8_t {
	E0 = 0,
	E1 = 1,
	E2 = 2,
	E3 = 3,
	E4 = 4,
	E5 = 5,
	E6 = 6
};

enum class ClimateContinentalness : uint8_t {
	MUSHROOM = 0,
	OCEAN = 1,
	COAST = 2,
	NEAR_INLAND = 3,
	MID_INLAND = 4,
	FAR_INLAND = 5
};

enum class ClimatePeaksValleys : uint8_t {
	VALLEY = 0,
	LOW = 1,
	MID = 2,
	HIGH = 3,
	PEAK = 4
};

// Maybe careful in the futur to not break the ordering of blocks for world saves : append new blocks at the end
enum class BlockType : ItemID {
	BEGIN = 0,
	AIR,
    GRASS,
	DIRT,
    STONE,
    SAND,
    SNOW,
    WATER,
    BEDROCK,
    OAK_LOG,
    OAK_LEAVES,
	IRON,
	GOLD,
	DIAMOND,
	URANIUM,
	LAVA,
	SHORT_GRASS,
	CORNFLOWER,
	POPPY,
	PINK_TULIP,
	ORANGE_TULIP,
	RED_TULIP,
	WHITE_TULIP,
	BLUE_ORCHID,
	ALLIUM,
	AZURE_BLUET,
	OXEYE_DAISY,
	LILY_OF_THE_VALLEY,
	WITHER_ROSE,
	DANDELION,
	RED_MUSHROOM,
	BROWN_MUSHROOM,
	SEAGRASS,
	TALL_SEAGRASS_BOTTOM,
	TALL_SEAGRASS_TOP,
	KELP,
	KELP_PLANT,
	BRAIN_CORAL,
	BRAIN_CORAL_FAN,
	BUBBLE_CORAL,
	BUBBLE_CORAL_FAN,
	FIRE_CORAL,
	FIRE_CORAL_FAN,
	HORN_CORAL,
	HORN_CORAL_FAN,
	TUBE_CORAL,
	TUBE_CORAL_FAN,
	DEAD_BUSH,
	CACTUS,
	TERRACOTTA,
	RED_TERRACOTTA,
	GRAY_TERRACOTTA,
	PINK_TERRACOTTA,
	BLACK_TERRACOTTA,
	BROWN_TERRACOTTA,
	WHITE_TERRACOTTA,
	ORANGE_TERRACOTTA,
	YELLOW_TERRACOTTA,
	LIGHT_GRAY_TERRACOTTA,
	BIRCH_LEAVES,
	ACACIA_LEAVES,
	JUNGLE_LEAVES,
	SPRUCE_LEAVES,
	DARK_OAK_LEAVES,
	BIRCH_LOG,
	ACACIA_LOG,
	JUNGLE_LOG,
	SPRUCE_LOG,
	DARK_OAK_LOG,
	ICE,
	PACKED_ICE,
	BLUE_ICE,
	BASALT,
	BLACKSTONE,
	CLAY,
	COARSE_DIRT,
	COBBLESTONE,
	GRAVEL,
	MOSSY_COBBLESTONE,
	CRYING_OBSIDIAN,
	NETHERRACK,
	SOUL_SAND,
	CRIMSON_NYLIUM,
	RED_SAND,
	RED_SANDSTONE,
	ANDESITE,
	GRANITE,
	DIORITE,
	RED_MUSHROOM_BLOCK,
	SANDSTONE,
	END
};

enum class WeaponType : ItemID {
	BEGIN = (ItemID)BlockType::END + 1,
	SWORD,
	END
};

enum class MiscType : ItemID {
	BEGIN = (ItemID)WeaponType::END + 1,
	IRON_INGOT,
	GOLD_INGOT,
	DIAMOND,
	END
};

using ItemType = std::variant<BlockType, WeaponType, MiscType>;

struct BlockDef { uint8_t toughness; };
struct LiquidDef { uint8_t maxPropagation; };
// texturePath is the base name (without extension) of a PNG in assets/textures/item/.
// Sprite items are rendered as flat 2D quads both in the world and in the inventory.
struct WeaponDef { std::string texturePath; int damage; };
struct MiscDef    { std::string texturePath; };

using ItemData = std::variant<BlockDef, LiquidDef, WeaponDef, MiscDef>;

struct ItemDef {
    ItemType id;
    std::string name;
    ItemData data;
};

class ItemRegistry {
public:
    // Factory helpers
    static ItemDef makeBlock(BlockType id, std::string name, uint8_t toughness) {
        return { id, std::move(name), BlockDef{ toughness } };
    }
    static ItemDef makeLiquid(BlockType id, std::string name, uint8_t maxProp) {
        return { id, std::move(name), LiquidDef{ maxProp } };
    }
    static ItemDef makeWeapon(WeaponType id, std::string name, std::string texPath, int dmg ) {
        return { id, std::move(name), WeaponDef{ std::move(texPath), dmg } };
    }
    static ItemDef makeMisc(MiscType id, std::string name, std::string texPath = "") {
        return { id, std::move(name), MiscDef{ std::move(texPath) } };
    }

	static inline std::vector<ItemDef> blocks = {
		ItemDef{ makeBlock(BlockType::AIR, 						"Air", 0) },
		ItemDef{ makeBlock(BlockType::GRASS,					"Grass", 10) },
		ItemDef{ makeBlock(BlockType::DIRT, 					"Dirt", 10) },
		ItemDef{ makeBlock(BlockType::STONE,	 				"Stone", 10) },
		ItemDef{ makeBlock(BlockType::SAND, 					"Sand", 10) },
		ItemDef{ makeBlock(BlockType::SNOW, 					"Snow", 10) },
		ItemDef{ makeLiquid(BlockType::WATER, 					"Water", 7) },
		ItemDef{ makeBlock(BlockType::BEDROCK, 					"Bedrock", 10) },
		ItemDef{ makeBlock(BlockType::OAK_LOG, 					"Log", 10) },
		ItemDef{ makeBlock(BlockType::BIRCH_LOG, 				"Birch Log", 10) },
		ItemDef{ makeBlock(BlockType::ACACIA_LOG, 				"Acacia Log", 10) },
		ItemDef{ makeBlock(BlockType::JUNGLE_LOG, 				"Jungle Log", 10) },
		ItemDef{ makeBlock(BlockType::SPRUCE_LOG, 				"Spruce Log", 10) },
		ItemDef{ makeBlock(BlockType::DARK_OAK_LOG, 			"Dark Oak Log", 10) },
		ItemDef{ makeBlock(BlockType::OAK_LEAVES, 				"Oak Leaves", 1) },
		ItemDef{ makeBlock(BlockType::BIRCH_LEAVES, 			"Birch Leaves", 1) },
		ItemDef{ makeBlock(BlockType::ACACIA_LEAVES, 			"Acacia Leaves", 1) },
		ItemDef{ makeBlock(BlockType::JUNGLE_LEAVES, 			"Jungle Leaves", 1) },
		ItemDef{ makeBlock(BlockType::SPRUCE_LEAVES, 			"Spruce Leaves", 1) },
		ItemDef{ makeBlock(BlockType::DARK_OAK_LEAVES, 			"Dark Oak Leaves", 1) },
		ItemDef{ makeBlock(BlockType::IRON, 					"Iron", 10) },
		ItemDef{ makeBlock(BlockType::GOLD, 					"Gold", 10) },
		ItemDef{ makeBlock(BlockType::DIAMOND, 					"Diamond", 10) },
		ItemDef{ makeBlock(BlockType::URANIUM, 					"Uranium", 10) },
		ItemDef{ makeLiquid(BlockType::LAVA, 					"Lava", 4) },
		ItemDef{ makeBlock(BlockType::SHORT_GRASS, 				"Short Grass", 1) },
		ItemDef{ makeBlock(BlockType::CORNFLOWER, 				"Cornflower", 1) },
		ItemDef{ makeBlock(BlockType::POPPY, 					"Poppy", 1) },
		ItemDef{ makeBlock(BlockType::PINK_TULIP, 				"Pink Tulip", 1) },
		ItemDef{ makeBlock(BlockType::ORANGE_TULIP, 			"Orange Tulip", 1) },
		ItemDef{ makeBlock(BlockType::RED_TULIP, 				"Red Tulip", 1) },
		ItemDef{ makeBlock(BlockType::WHITE_TULIP, 				"White Tulip", 1) },
		ItemDef{ makeBlock(BlockType::BLUE_ORCHID, 				"Blue Orchid", 1) },
		ItemDef{ makeBlock(BlockType::ALLIUM, 					"Allium", 1) },
		ItemDef{ makeBlock(BlockType::AZURE_BLUET, 				"Azure Bluet", 1) },
		ItemDef{ makeBlock(BlockType::OXEYE_DAISY, 				"Oxeye Daisy", 1) },
		ItemDef{ makeBlock(BlockType::LILY_OF_THE_VALLEY, 		"Lily of the Valley", 1) },
		ItemDef{ makeBlock(BlockType::WITHER_ROSE, 				"Wither Rose", 1) },
		ItemDef{ makeBlock(BlockType::DANDELION, 				"Dandelion", 1) },
		ItemDef{ makeBlock(BlockType::RED_MUSHROOM, 			"Red Mushroom", 1) },
		ItemDef{ makeBlock(BlockType::BROWN_MUSHROOM, 			"Brown Mushroom", 1) },
		ItemDef{ makeBlock(BlockType::DEAD_BUSH, 				"Dead Bush", 1) },
		ItemDef{ makeBlock(BlockType::CACTUS, 					"Cactus", 1) },
		ItemDef{ makeBlock(BlockType::TERRACOTTA, 				"Terracotta", 1) },
		ItemDef{ makeBlock(BlockType::RED_TERRACOTTA, 			"Red Terracotta", 10) },
		ItemDef{ makeBlock(BlockType::GRAY_TERRACOTTA, 			"Gray Terracotta", 10) },
		ItemDef{ makeBlock(BlockType::PINK_TERRACOTTA, 			"Pink Terracotta", 10) },
		ItemDef{ makeBlock(BlockType::BLACK_TERRACOTTA, 		"Black Terracotta", 10) },
		ItemDef{ makeBlock(BlockType::BROWN_TERRACOTTA, 		"Brown Terracotta", 10) },
		ItemDef{ makeBlock(BlockType::WHITE_TERRACOTTA, 		"White Terracotta", 10) },
		ItemDef{ makeBlock(BlockType::ORANGE_TERRACOTTA, 		"Orange Terracotta", 10) },
		ItemDef{ makeBlock(BlockType::YELLOW_TERRACOTTA, 		"Yellow Terracotta", 10) },
		ItemDef{ makeBlock(BlockType::LIGHT_GRAY_TERRACOTTA, 	"Light Gray Terracotta", 10) },
		ItemDef{ makeBlock(BlockType::ICE,               		"Ice", 1) },
		ItemDef{ makeBlock(BlockType::PACKED_ICE,        		"Packed Ice", 1) },
		ItemDef{ makeBlock(BlockType::BLUE_ICE,          		"Blue Ice", 1) },
		ItemDef{ makeBlock(BlockType::BASALT,          			"Basalt", 1) },
		ItemDef{ makeBlock(BlockType::BLACKSTONE,          		"Blackstone", 1) },
		ItemDef{ makeBlock(BlockType::CLAY,              		"Clay", 1) },
		ItemDef{ makeBlock(BlockType::COARSE_DIRT,       		"Coarse Dirt", 1) },
		ItemDef{ makeBlock(BlockType::COBBLESTONE,       		"Cobblestone", 1) },
		ItemDef{ makeBlock(BlockType::GRAVEL,            		"Gravel", 1) },
		ItemDef{ makeBlock(BlockType::MOSSY_COBBLESTONE, 		"Mossy Cobblestone", 1) },
		ItemDef{ makeBlock(BlockType::CRYING_OBSIDIAN,   		"Crying Obsidian", 1) },
		ItemDef{ makeBlock(BlockType::NETHERRACK,        		"Netherrack", 1) },
		ItemDef{ makeBlock(BlockType::SOUL_SAND,         		"Soul Sand", 1) },
		ItemDef{ makeBlock(BlockType::CRIMSON_NYLIUM,    		"Crimson Nylium", 1) },
		ItemDef{ makeBlock(BlockType::RED_SAND,          		"Red Sand", 1) },
		ItemDef{ makeBlock(BlockType::RED_SANDSTONE,          	"Red Sandstone", 1) },
		ItemDef{ makeBlock(BlockType::ANDESITE,          		"Andesite", 1) },
		ItemDef{ makeBlock(BlockType::GRANITE,           		"Granite", 1) },
		ItemDef{ makeBlock(BlockType::DIORITE,           		"Diorite", 1) },
		ItemDef{ makeBlock(BlockType::RED_MUSHROOM_BLOCK, 		"Red Mushroom Block", 1) },
		ItemDef{ makeBlock(BlockType::SANDSTONE, 				"Sandstone", 1) },
	};

	// static inline std::vector<ItemDef> liquids = {
	// 	ItemDef{ makeLiquid(BlockType::WATER, "Water", 7) },
		
	// };

	static inline std::vector<ItemDef> weapons = {
		// texturePath = PNG base name in assets/textures/item/ (no extension).
		// Empty string = no texture; drop e.g. wooden_sword.png in that folder
		// and change "" to "wooden_sword" to get an icon and dropped sprite.
		ItemDef{ makeWeapon(WeaponType::SWORD, "Sword", "", 4) }
	};

	// To add a new item (ingot, stick, etc):
	//   1. Add an entry to MiscType above (between BEGIN and END).
	//   2. Drop the PNG in assets/textures/item/.
	//   3. Append a line here with makeMisc(MiscType::X, "Display Name", "tex_name").
	// The texture is auto-loaded by TextureManager and the item renders as a
	// flat 2D sprite in the world and the inventory.
	static inline std::vector<ItemDef> miscs = {
		ItemDef{ makeMisc(MiscType::IRON_INGOT, "Iron Ingot", "iron_ingot") },
		ItemDef{ makeMisc(MiscType::GOLD_INGOT, "Gold Ingot", "gold_ingot") },
		ItemDef{ makeMisc(MiscType::DIAMOND,    "Diamond",    "diamond")    },
	};

	static inline std::vector<ItemDef> items = [] {
		std::vector<ItemDef> v;
		v.insert(v.end(), blocks.begin(), blocks.end());
		// v.insert(v.end(), liquids.begin(), liquids.end());
		v.insert(v.end(), weapons.begin(), weapons.end());
		v.insert(v.end(), miscs.begin(), miscs.end());
		return v;
	}();

	inline static const ItemDef& get(const ItemType& id) {
		return std::visit([](auto&& arg) -> const ItemDef& {
			return items[static_cast<ItemID>(arg)];
		}, id);
	}

	inline static const ItemDef& getBlock(const BlockType& id) {
		return blocks[static_cast<ItemID>(id) - static_cast<ItemID>(BlockType::BEGIN) - 1];
	}

	inline static const ItemDef& getLiquid(BlockType id) {
		return blocks[static_cast<ItemID>(id) - static_cast<ItemID>(BlockType::BEGIN) - 1];
	}


	inline static const ItemDef& getWeapon(const WeaponType& id) {
		return weapons[static_cast<ItemID>(id) - static_cast<ItemID>(WeaponType::BEGIN)];
	}
};

// If needed, here's the list of all vegetation blocks for quick reference
inline static constexpr BlockType VEGETATION_BLOCKS[] = {
	BlockType::SHORT_GRASS,
	BlockType::CORNFLOWER,
	BlockType::POPPY,
	BlockType::PINK_TULIP,
	BlockType::ORANGE_TULIP,
	BlockType::RED_TULIP,
	BlockType::WHITE_TULIP,
	BlockType::BLUE_ORCHID,
	BlockType::ALLIUM,
	BlockType::AZURE_BLUET,
	BlockType::OXEYE_DAISY,
	BlockType::LILY_OF_THE_VALLEY,
	BlockType::WITHER_ROSE,
	BlockType::DANDELION,
	BlockType::RED_MUSHROOM,
	BlockType::BROWN_MUSHROOM,
	BlockType::SEAGRASS,
	BlockType::TALL_SEAGRASS_BOTTOM,
	BlockType::TALL_SEAGRASS_TOP,
	BlockType::KELP,
	BlockType::KELP_PLANT,
	BlockType::BRAIN_CORAL,
	BlockType::BRAIN_CORAL_FAN,
	BlockType::BUBBLE_CORAL,
	BlockType::BUBBLE_CORAL_FAN,
	BlockType::FIRE_CORAL,
	BlockType::FIRE_CORAL_FAN,
	BlockType::HORN_CORAL,
	BlockType::HORN_CORAL_FAN,
	BlockType::TUBE_CORAL,
	BlockType::TUBE_CORAL_FAN,
	BlockType::DEAD_BUSH,

};

// If needed, here's the list of sea vegetation blocks only
inline static constexpr BlockType SEA_VEGETATION_BLOCKS[] = {
	BlockType::SEAGRASS,
	BlockType::TALL_SEAGRASS_BOTTOM,
	BlockType::TALL_SEAGRASS_TOP,
	BlockType::KELP,
	BlockType::KELP_PLANT,
	BlockType::BRAIN_CORAL,
	BlockType::BRAIN_CORAL_FAN,
	BlockType::BUBBLE_CORAL,
	BlockType::BUBBLE_CORAL_FAN,
	BlockType::FIRE_CORAL,
	BlockType::FIRE_CORAL_FAN,
	BlockType::HORN_CORAL,
	BlockType::HORN_CORAL_FAN,
	BlockType::TUBE_CORAL,
	BlockType::TUBE_CORAL_FAN,
};

inline static bool isSeaVegetation(const BlockType &b) {
	switch(b) {
		case BlockType::SEAGRASS:
		case BlockType::TALL_SEAGRASS_BOTTOM:
		case BlockType::TALL_SEAGRASS_TOP:
		case BlockType::KELP:
		case BlockType::KELP_PLANT:
		case BlockType::BRAIN_CORAL:
		case BlockType::BRAIN_CORAL_FAN:
		case BlockType::BUBBLE_CORAL:
		case BlockType::BUBBLE_CORAL_FAN:
		case BlockType::FIRE_CORAL:
		case BlockType::FIRE_CORAL_FAN:
		case BlockType::HORN_CORAL:
		case BlockType::HORN_CORAL_FAN:
		case BlockType::TUBE_CORAL:
		case BlockType::TUBE_CORAL_FAN:
			return true;
		default:
			return false;
	}
}

// Sea vegetation that grows in stacks (kelp, tall seagrass), as opposed to single-block plants
inline static bool isStackableSeaVegetation(const BlockType &b) {
	switch (b) {
		case BlockType::KELP:
		case BlockType::KELP_PLANT:
		case BlockType::TALL_SEAGRASS_BOTTOM:
		case BlockType::TALL_SEAGRASS_TOP:
			return true;
		default:
			return false;
	}
}

inline static bool isBlockVegetation(const BlockType &b) {
	switch(b) {
		case BlockType::SHORT_GRASS:
		case BlockType::CORNFLOWER:
		case BlockType::POPPY:
		case BlockType::PINK_TULIP:
		case BlockType::ORANGE_TULIP:
		case BlockType::RED_TULIP:
		case BlockType::WHITE_TULIP:
		case BlockType::BLUE_ORCHID:
		case BlockType::ALLIUM:
		case BlockType::AZURE_BLUET:
		case BlockType::OXEYE_DAISY:
		case BlockType::LILY_OF_THE_VALLEY:
		case BlockType::WITHER_ROSE:
		case BlockType::DANDELION:
		case BlockType::RED_MUSHROOM:
		case BlockType::BROWN_MUSHROOM:
		case BlockType::DEAD_BUSH:
			return true;
		default:
			return false;
	}
}

inline static bool isBlockTransparent(const BlockType &b) {
	switch (b) {
		case BlockType::OAK_LEAVES:
		case BlockType::BIRCH_LEAVES:
		case BlockType::ACACIA_LEAVES:
		case BlockType::JUNGLE_LEAVES:
		case BlockType::SPRUCE_LEAVES:
		case BlockType::DARK_OAK_LEAVES:
		case BlockType::CACTUS:
			return true;
		default:
			return false;
	}
}

inline static bool isBlockLeaves(const BlockType &b) {
	switch (b) {
		case BlockType::OAK_LEAVES:
		case BlockType::BIRCH_LEAVES:
		case BlockType::ACACIA_LEAVES:
		case BlockType::JUNGLE_LEAVES:
		case BlockType::SPRUCE_LEAVES:
		case BlockType::DARK_OAK_LEAVES:
			return true;
		default:
			return false;
	}
}

inline static bool isBlockSolid(const BlockType &b) {
	return b != BlockType::AIR && b != BlockType::WATER && !isBlockVegetation(b);
}

// True when the item should be rendered as a flat 2D sprite rather than a 3D
// cube. Used both for dropped items in the world and for inventory icons.
// Sprites: vegetation/coral blocks, all weapons, all misc items.
inline static bool isItemFlat(const ItemType &t) {
	if (auto* b = std::get_if<BlockType>(&t)) {
		return isBlockVegetation(*b) || isSeaVegetation(*b);
	}
	// WeaponType and MiscType are always flat sprites.
	return true;
}

template<typename Enum>
constexpr bool inRange(ItemID id)
{
    return id > (ItemID)Enum::BEGIN && id < (ItemID)Enum::END;
}

inline static ItemID itemTypeToItemID(ItemType type) {
	return std::visit([](auto& value) -> ItemID {
		return static_cast<ItemID>(value);
	}, type);
}

//contexpr alternative to the above function
// constexpr ItemID itemTypeToItemID(const ItemType& type) {
//     switch (type.index()) {
//         case 0: // BlockType
//             return static_cast<ItemID>(std::get<0>(type));
// 		case 1: // WeaponType
// 			return static_cast<ItemID>(std::get<1>(type));
//         case 2: // MiscType
//             return static_cast<ItemID>(std::get<2>(type));
//         default:
//             return 0; // or assert
//     }
// }

inline static ItemType itemIDToItemType(ItemID id)
{
    if (inRange<BlockType>(id))  return static_cast<BlockType>(id);
    if (inRange<WeaponType>(id)) return static_cast<WeaponType>(id);
    if (inRange<MiscType>(id))   return static_cast<MiscType>(id);

    return BlockType::BEGIN;
}

#endif