
#ifndef ITEM_HPP
#define ITEM_HPP

#include <cstdint>
#include <string>
#include <variant>
#include <array>

using TypeID = uint8_t;   // for world storage/networking
using ItemID  = uint16_t;  // for inventory/items

enum class BlockType : TypeID {
    AIR = 0,
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

enum class LiquidType : TypeID {
	WATER = 0, //BlockType::Leaves + 1?
	LAVA
};

enum class WeaponType : TypeID {
	SWORD = 0,
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
    static const ItemDef& get(uint16_t id) { return items[id]; }
	static const ItemDef& getBlocks(uint16_t id) { return blocks[id]; }
	static const ItemDef& getLiquids(uint16_t id) { return liquids[id]; }
	static const ItemDef& getWeapons(uint16_t id) { return weapons[id]; }


    // static inline std::array<ItemDef, 256> items = {
    //     ItemDef{ 0, "Air", BlockDef{.textureCoords = 0}},
    //     ItemDef{ 1, ItemDef::Block, .block = {1}, "Grass" },
    //     ItemDef{ 2, ItemDef::Liquid, .liquid = {7}, "Water" },
    //     ItemDef{ 3, ItemDef::Weapon, .weapon = {5}, "Sword", "textures/sword.png" },
    //     // ...
    // };

    // Factory helpers
    static ItemDef makeBlock(TypeID id, std::string name, uint8_t toughness) {
        return { id, std::move(name), BlockDef{ toughness } };
    }
    static ItemDef makeLiquid(TypeID id, std::string name, int maxProp) {
        return { id, std::move(name), LiquidDef{ maxProp } };
    }
    static ItemDef makeWeapon(TypeID id, std::string name, std::string texPath, int dmg ) {
        return { id, std::move(name), WeaponDef{ std::move(texPath), dmg } };
    }
    static ItemDef makeMisc(TypeID id, std::string name) {
        return { id, std::move(name), MiscDef{} };
    }

	static inline std::array<ItemDef, 256> blocks = {
		ItemDef{ makeBlock((TypeID)BlockType::AIR, "Air", 0) },
		ItemDef{ makeBlock((TypeID)BlockType::GRASS, "Grass", 10) },
		ItemDef{ makeBlock((TypeID)BlockType::STONE, "Stone", 10) },
		ItemDef{ makeBlock((TypeID)BlockType::SAND, "Sand", 10) }, 
		ItemDef{ makeBlock((TypeID)BlockType::SNOW, "Snow", 10) },
		ItemDef{ makeBlock((TypeID)BlockType::WATER, "Water", 10) }, //water is a liquid. So TODO : REMOVE IT
		ItemDef{ makeBlock((TypeID)BlockType::DIRT, "Dirt", 10) },
		ItemDef{ makeBlock((TypeID)BlockType::BEDROCK, "Bedrock", 10) },
		ItemDef{ makeBlock((TypeID)BlockType::LOG, "Log", 10) },
		ItemDef{ makeBlock((TypeID)BlockType::LEAVES, "Leaves", 10) }
	};

	static inline std::array<ItemDef, 256> liquids = {
		ItemDef{ makeLiquid((TypeID)LiquidType::WATER, "Water", 7) },
		ItemDef{ makeLiquid((TypeID)LiquidType::LAVA, "Lava", 4) },
	};

	static inline std::array<ItemDef, 256> weapons = {
		ItemDef{ makeWeapon((TypeID)WeaponType::SWORD, "Sword", "", 4) }
	}
};


#endif