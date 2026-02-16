#include "Inventory.hpp"

Inventory::Inventory()
{
	for (int i = 0; i < rows * cols; i++)
		freeSlots.insert(i);
}

std::pair<ItemType, uint8_t> Inventory::getSlot(uint8_t slot)
{
	return grid.at(slot); 
}

ItemType Inventory::getItemAtSlot(int slot)
{
	return grid[slot].first;
}

//only works with inserts of 1 actually. being able to insert more isn't and probably won't ever be support
//returns the slot which has been used to insert the item. 0 in case insertion was not successful.
int Inventory::insertItems(ItemType item, int amount) //maybe take a reference to amount so sender can know how many items couldn't fit in slot. TODO?
{
	auto itemSlot = itemsIndexes.find(item);
	if (itemSlot != itemsIndexes.end())
	{
		if (itemSlot->second + amount <= MAX_STACK_SIZE)
		{
			grid[itemSlot->second].second += amount;
			return itemSlot->second;
		}
	}

	int slot = getFirstFreeSlot();
	if (slot == -1) return false;

	freeSlots.erase(slot);
	grid[slot] = {item, amount};
	itemsIndexes.insert({item, slot});

	return slot;
}

bool Inventory::removeItemsFromSlot(int slotNumber, int amount)
{
	if (amount > grid[slotNumber].second) return false;

	grid[slotNumber].second = grid[slotNumber].second - amount;

	if (grid[slotNumber].second == 0)
	{
		grid[slotNumber] = {};
		freeSlots.insert(slotNumber);
		
		auto range = itemsIndexes.equal_range(grid[slotNumber].first);
		for (auto it = range.first; it != range.second; ++it)
		{
			if (it->second == slotNumber)
			{
				itemsIndexes.erase(it);
				break;
			}
		}
	}

	return true;
}

bool Inventory::insertItemsToSlot(ItemType item, int slotNumber, int amount)
{
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
	if (amount > MAX_STACK_SIZE - currAmount)
		grid[slotNumber] = {item, MAX_STACK_SIZE};
	else
		grid[slotNumber] = {item, currAmount + amount};

	return true;
}
