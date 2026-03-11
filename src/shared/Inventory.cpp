#include "Inventory.hpp"

//MOST OF THESE FUNCTIONS WON'T WORK ONCE WE ADD OTHER TYPES OF ITEMS. FF
Inventory::Inventory()
{
	for (int i = 0; i < rows * cols; i++)
		freeSlots.insert(i);
}

std::pair<ItemType, itemStackSize_t> Inventory::getSlot(int slot)
{
	return grid.at(slot); 
}

ItemType Inventory::getItemAtSlot(int slot)
{
	if (slot >= grid.size() || slot < 0) return BlockType::BEGIN;
	return grid[slot].first;
}

ItemID Inventory::getItemIDAtSlot(int slot)
{
	if (slot >= grid.size() || slot < 0) return static_cast<std::underlying_type_t<BlockType>>(BlockType::BEGIN);
	return std::visit([](auto v) -> ItemID {
        return static_cast<ItemID>(v);
    }, grid[slot].first);
}

ItemType Inventory::getActiveItem()
{
	return grid[activeHotbarSlot].first;
}

ItemID Inventory::getActiveItemID()
{
	return std::visit([](auto v) -> ItemID {
        return static_cast<ItemID>(v);
    }, grid[activeHotbarSlot].first);
}

void Inventory::setSlot(int slot, itemStackSize_t amount, ItemType type)
{
	if (slot >= grid.size() || slot < 0) return ;

	// If amount is zero we must clear the slot regardless of the new type.
	if (amount == 0)
	{
		// remove old index if present
		auto range = itemsIndexes.equal_range(grid[slot].first);
		for (auto it = range.first; it != range.second; ++it)
		{
			if (it->second == slot)
			{
				itemsIndexes.erase(it);
				break;
			}
		}
		grid[slot] = {};
		freeSlots.insert(slot);
		return;
	}

	// If type changed, remove the old index and add the new one
	if (type != grid[slot].first)
	{
		auto range = itemsIndexes.equal_range(grid[slot].first);
		for (auto it = range.first; it != range.second; ++it)
		{
			if (it->second == slot)
			{
				itemsIndexes.erase(it);
				break;
			}
		}
		itemsIndexes.insert({ type, slot });
	}

	if (freeSlots.count(slot))
		freeSlots.erase(slot);

	grid[slot] = {type, amount};
}

void Inventory::setSlot(int slot, itemStackSize_t amount, ItemID t)
{
	auto type = itemIDToItemType(t);

	setSlot(slot, amount, type);
}

//returns the slot which has been used to insert the item. -1 in case insertion was not successful.
//if amount is still > 0, caller can just recall the function
int Inventory::insertItems(ItemType item, int &amount)
{
	auto itemSlots = itemsIndexes.equal_range(item);

	for(auto itemSlot = itemSlots.first; itemSlot != itemSlots.second; itemSlot++)
	{
		auto& stack = grid[itemSlot->second].second;

		if (stack >= MAX_STACK_SIZE)
			continue;

		int inserted = std::min<int>(MAX_STACK_SIZE - stack, amount);

		stack += inserted;
		amount -= inserted;

		return itemSlot->second;
	}

	int slot = getFirstFreeSlot();
	if (slot == -1) return INVALID_SLOT;

	freeSlots.erase(slot);

	int inserted = std::min<int>(MAX_STACK_SIZE, amount);
	grid[slot] = {item, inserted};
	amount -= inserted;
	itemsIndexes.insert({item, slot});

	return slot;
}

bool Inventory::removeItemsFromSlot(int slotNumber, itemStackSize_t amount)
{
	if (slotNumber >= grid.size() || slotNumber < 0) return false;
	if (amount > grid[slotNumber].second) return false;

	grid[slotNumber].second = grid[slotNumber].second - amount;

	if (grid[slotNumber].second == 0)
	{		
		auto range = itemsIndexes.equal_range(grid[slotNumber].first);
		for (auto it = range.first; it != range.second; ++it)
		{
			if (it->second == slotNumber)
			{
				itemsIndexes.erase(it);
				break;
			}
		}

		grid[slotNumber] = {};
		freeSlots.insert(slotNumber);
	}

	return true;
}

//inserted items must have the same type as the item in the slot
bool Inventory::insertItemsToSlot(ItemType item, int slotNumber, int &amount)
{
	if (slotNumber >= grid.size() || slotNumber < 0) return false;
	if (grid[slotNumber].second > 0 && grid[slotNumber].first != item) return false; //not same type

	//check if item - slotNumber is already registered.
	auto range = itemsIndexes.equal_range(item);
	bool exists = false;

	for (auto it = range.first; it != range.second; ++it)
	{
		if (it->second == slotNumber)
		{
			exists = true;
			break;
		}
	}

	if (!exists)
		itemsIndexes.insert({item, slotNumber});

	if (freeSlots.count(slotNumber))
		freeSlots.erase(slotNumber);

	//insert item
	uint8_t currAmount = grid[slotNumber].second;
	int inserted = std::min<int>(MAX_STACK_SIZE - currAmount, amount);
	grid[slotNumber] = {item, currAmount + inserted};
	amount -= inserted;
	return true;
}

bool Inventory::insertItemsToSlot(ItemID i, int slotNumber, int &amount)
{
	ItemType item = itemIDToItemType(i);
	return insertItemsToSlot(item, slotNumber, amount);
}

void Inventory::mergeSlot(int slotSrc, int slotDest)
{
	if (slotSrc >= grid.size() || slotSrc < 0) return ;
	if (slotDest >= grid.size() || slotDest < 0) return ;

	ItemType type = getSlot(slotSrc).first;
	auto dst = getSlot(slotDest);

	if (type != dst.first && dst.second != 0) return ;

	int amount = getSlot(slotSrc).second;

	insertItemsToSlot(type, slotDest, amount);
	if (amount > 0)
		setSlot(slotSrc, amount, type);
	else
		setSlot(slotSrc, 0, 0);
}

void Inventory::takeOneItemFromSlot(int slotSrc, std::optional<int> slotDest)
{
	if (slotSrc >= grid.size() || slotSrc < 0) return ;
	if (slotDest.has_value() && (*slotDest >= grid.size() || *slotDest < 0)) return ;

	auto slot = getSlot(slotSrc);
	int amount = slot.second;
	ItemType type = slot.first;
	if (amount == 0) return ;
	if (slotDest.has_value() &&  getSlot(*slotDest).second != 0 && getSlot(*slotDest).first != type) return ;

	bool inserted = false;
	int one = 1;
	if (slotDest.has_value())
		inserted = insertItemsToSlot(type, slotDest.value(), one);

	if (!inserted) return;
	if (amount - 1 > 0)
		setSlot(slotSrc, amount - 1, type);
	else
		setSlot(slotSrc, 0, 0);

}


void Inventory::takeHalf(int slotSrc)
{
	if (slotSrc >= grid.size() || slotSrc < 0) return ;

	ItemType type = getSlot(slotSrc).first;

	if (getHand().second != 0) return ; //technically shouldn't trigger this

	int amount = getSlot(slotSrc).second / 2;
	int cpy = amount;

	if (getSlot(slotSrc).second % 2 != 0)
		amount++;

	insertItemsToSlot(type, HAND_ID, amount);

	if (getSlot(slotSrc).second - amount > 0)
		setSlot(slotSrc, cpy + amount, type);
	else
		setSlot(slotSrc, 0, 0);
}

void Inventory::swapSlots(int slot1, int slot2)
{
	if (slot1 >= grid.size() || slot1 < 0) return ;
	if (slot2 >= grid.size() || slot2 < 0) return ;

	auto tempSlot = getSlot(slot1);
	auto s = getSlot(slot2);

	setSlot(slot1, s.second, s.first);
	setSlot(slot2, tempSlot.second, tempSlot.first);
}