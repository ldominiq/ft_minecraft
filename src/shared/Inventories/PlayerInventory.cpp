
#include "PlayerInventory.hpp"

PlayerInventory::PlayerInventory(std::shared_ptr<InventoryExternalVariablesRefs> inventoryExternalVarsRefs) : Inventory<4, 9, 1>(inventoryExternalVarsRefs)
{
	type = InventoryType::PLAYER;
}

ItemType PlayerInventory::getActiveItem()
{
	return grid[activeHotbarSlot].first;
}

ItemID PlayerInventory::getActiveItemID()
{
	return std::visit([](auto v) -> ItemID {
        return static_cast<ItemID>(v);
    }, grid[activeHotbarSlot].first);
}