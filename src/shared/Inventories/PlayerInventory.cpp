
#include "PlayerInventory.hpp"

PlayerInventory::PlayerInventory(std::shared_ptr<std::pair<ItemType, itemStackSize_t>> handPtr) : Inventory<4, 9>(handPtr)
{
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