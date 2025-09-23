
#ifndef ITEM_HPP
#define ITEM_HPP

#include <cstdint>
#include <string>
#include <variant>
#include <array>
#include <vector>

// using ItemID = uint8_t;   // for world storage/networking
using ItemID  = uint16_t;  // for inventory/items

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
	END
};

enum class LiquidType : ItemID {
	BEGIN = (ItemID)BlockType::END + 1,
	WATER,
	LAVA,
	END
};

enum class WeaponType : ItemID {
	BEGIN = (ItemID)LiquidType::END + 1,
	SWORD,
	END
};

enum class MiscType : ItemID {
	BEGIN = (ItemID)WeaponType::END + 1,
	END
};

struct BlockDef { uint8_t toughness; };
struct LiquidDef { int maxPropagation; };
struct WeaponDef { std::string texturePath; int damage; };
struct MiscDef {};

using ItemData = std::variant<BlockDef, LiquidDef, WeaponDef, MiscDef>;

struct ItemDef {
    ItemID id;
    std::string name;
    ItemData data;
};

class ItemRegistry {
public:
    // Factory helpers
    static ItemDef makeBlock(ItemID id, std::string name, uint8_t toughness) {
        return { id, std::move(name), BlockDef{ toughness } };
    }
    static ItemDef makeLiquid(ItemID id, std::string name, int maxProp) {
        return { id, std::move(name), LiquidDef{ maxProp } };
    }
    static ItemDef makeWeapon(ItemID id, std::string name, std::string texPath, int dmg ) {
        return { id, std::move(name), WeaponDef{ std::move(texPath), dmg } };
    }
    static ItemDef makeMisc(ItemID id, std::string name) {
        return { id, std::move(name), MiscDef{} };
    }

	static inline std::array<ItemDef, 256> blocks = {
		ItemDef{ makeBlock((ItemID)BlockType::AIR, "Air", 0) },
		ItemDef{ makeBlock((ItemID)BlockType::GRASS, "Grass", 10) },
		ItemDef{ makeBlock((ItemID)BlockType::STONE, "Stone", 10) },
		ItemDef{ makeBlock((ItemID)BlockType::SAND, "Sand", 10) }, 
		ItemDef{ makeBlock((ItemID)BlockType::SNOW, "Snow", 10) },
		ItemDef{ makeBlock((ItemID)BlockType::WATER, "Water", 10) }, //water is a liquid. So TODO : REMOVE IT
		ItemDef{ makeBlock((ItemID)BlockType::DIRT, "Dirt", 10) },
		ItemDef{ makeBlock((ItemID)BlockType::BEDROCK, "Bedrock", 10) },
		ItemDef{ makeBlock((ItemID)BlockType::LOG, "Log", 10) },
		ItemDef{ makeBlock((ItemID)BlockType::LEAVES, "Leaves", 10) }
	};

	static inline std::array<ItemDef, 256> liquids = {
		ItemDef{ makeLiquid((ItemID)LiquidType::WATER, "Water", 7) },
		ItemDef{ makeLiquid((ItemID)LiquidType::LAVA, "Lava", 4) },
	};

	static inline std::array<ItemDef, 256> weapons = {
		ItemDef{ makeWeapon((ItemID)WeaponType::SWORD, "Sword", "", 4) }
	};

	static inline std::vector<ItemData> items = [] {
		std::vector<ItemData> v;
		v.insert(v.end(), blocks.begin(), blocks.end());
		v.insert(v.end(), liquids.begin(), liquids.end());
		v.insert(v.end(), weapons.begin(), weapons.end());
		return v;
	}();

    static const ItemData& get(uint16_t id) { return items[id]; }
	static const ItemDef& getBlock(uint16_t id) { return blocks[id]; }
	static const ItemDef& getLiquid(uint16_t id) { return liquids[id]; }
	static const ItemDef& getWeapon(uint16_t id) { return weapons[id]; }
};


#endif