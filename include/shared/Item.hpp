
#ifndef ITEM_HPP
#define ITEM_HPP

#include <cstdint>
#include <string>
#include <variant>
#include <array>
#include <vector>

using ItemID  = uint16_t;  // for inventory/items

enum class BiomeType {
    PLAINS,
    DESERT,
    FOREST,
    TUNDRA,
	SWAMP,
	OCEAN,
	MOUNTAIN
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
    LOG,
    LEAVES,
	IRON,
	GOLD,
	DIAMOND,
	URANIUM,
	LAVA,
	END
};

enum class WeaponType : ItemID {
	BEGIN = (ItemID)BlockType::END + 1,
	SWORD,
	END
};

enum class MiscType : ItemID {
	BEGIN = (ItemID)WeaponType::END + 1,
	END
};

using ItemType = std::variant<BlockType, WeaponType, MiscType>;

struct BlockDef { uint8_t toughness; };
struct LiquidDef { uint8_t maxPropagation; };
struct WeaponDef { std::string texturePath; int damage; };
struct MiscDef {};

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
    static ItemDef makeMisc(MiscType id, std::string name) {
        return { id, std::move(name), MiscDef{} };
    }

	static inline std::vector<ItemDef> blocks = {
		ItemDef{ makeBlock(BlockType::AIR, "Air", 0) },
		ItemDef{ makeBlock(BlockType::GRASS, "Grass", 10) },
		ItemDef{ makeBlock(BlockType::DIRT, "Dirt", 10) },
		ItemDef{ makeBlock(BlockType::STONE, "Stone", 10) },
		ItemDef{ makeBlock(BlockType::SAND, "Sand", 10) }, 
		ItemDef{ makeBlock(BlockType::SNOW, "Snow", 10) },
		ItemDef{ makeLiquid(BlockType::WATER, "Water", 7) },
		ItemDef{ makeBlock(BlockType::BEDROCK, "Bedrock", 10) },
		ItemDef{ makeBlock(BlockType::LOG, "Log", 10) },
		ItemDef{ makeBlock(BlockType::LEAVES, "Leaves", 10) },
		ItemDef{ makeBlock(BlockType::IRON, "Iron", 10) },
		ItemDef{ makeBlock(BlockType::GOLD, "Gold", 10) },
		ItemDef{ makeBlock(BlockType::DIAMOND, "Diamond", 10) },
		ItemDef{ makeBlock(BlockType::URANIUM, "Uranium", 10) },
		ItemDef{ makeLiquid(BlockType::LAVA, "Lava", 4) },
	};

	// static inline std::vector<ItemDef> liquids = {
	// 	ItemDef{ makeLiquid(BlockType::WATER, "Water", 7) },
		
	// };

	static inline std::vector<ItemDef> weapons = {
		ItemDef{ makeWeapon(WeaponType::SWORD, "Sword", "", 4) }
	};

	static inline std::vector<ItemDef> items = [] {
		std::vector<ItemDef> v;
		v.insert(v.end(), blocks.begin(), blocks.end());
		// v.insert(v.end(), liquids.begin(), liquids.end());
		v.insert(v.end(), weapons.begin(), weapons.end());
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

inline static bool isBlockSolid(const BlockType &b) { return b != BlockType::AIR && b != BlockType::WATER; }

template<typename Enum>
constexpr bool inRange(ItemID id)
{
    return id > (ItemID)Enum::BEGIN && id < (ItemID)Enum::END;
}

inline static ItemType itemIDToItemType(ItemID id)
{
    if (inRange<BlockType>(id))  return static_cast<BlockType>(id);
    if (inRange<WeaponType>(id)) return static_cast<WeaponType>(id);
    if (inRange<MiscType>(id))   return static_cast<MiscType>(id);

    return BlockType::BEGIN;
}

#endif