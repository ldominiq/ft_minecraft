#include "Inventory.hpp"

//MOST OF THESE FUNCTIONS WON'T WORK ONCE WE ADD OTHER TYPES OF ITEMS. FF
template<int ROWS, int COLS>
Inventory<ROWS, COLS>::Inventory(std::shared_ptr<std::pair<ItemType, itemStackSize_t>> handPtr)
{
	for (int i = 0; i < rows * cols; i++)
		freeSlots.insert(i);

	HAND_ID = grid.size() - 1; //the last slot of the grid is reserved for the hand.
	if (handPtr)
		hand = handPtr;
}

template<int ROWS, int COLS>
std::pair<ItemType, itemStackSize_t> Inventory<ROWS, COLS>::getSlot(int slot)
{
	return grid.at(slot);
}

template<int ROWS, int COLS>
ItemType Inventory<ROWS, COLS>::getItemAtSlot(int slot)
{
	if (slot >= grid.size() || slot < 0) return BlockType::BEGIN;
	return grid[slot].first;
}

template<int ROWS, int COLS>
ItemID Inventory<ROWS, COLS>::getItemIDAtSlot(int slot)
{
	if (slot >= grid.size() || slot < 0) return static_cast<std::underlying_type_t<BlockType>>(BlockType::BEGIN);
	return std::visit([](auto v) -> ItemID {
        return static_cast<ItemID>(v);
    }, grid[slot].first);
}

template<int ROWS, int COLS>
void Inventory<ROWS, COLS>::setSlot(int slot, itemStackSize_t amount, ItemType type)
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
		if (slot != HAND_ID)
			freeSlots.insert(slot);

		if (hand.lock())
			*hand.lock() = getHand();
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

	if (hand.lock())
		*hand.lock() = getHand();
}

template<int ROWS, int COLS>
void Inventory<ROWS, COLS>::setSlot(int slot, itemStackSize_t amount, ItemID t)
{
	auto type = itemIDToItemType(t);

	setSlot(slot, amount, type);
}

template<int ROWS, int COLS>
void Inventory<ROWS, COLS>::handleInventoryAction(NetInventoryAction &pkt)
{
	int slot = pkt.slot;

	//we syncronize the the hand with other inventories first.
	auto h = hand.lock();
	if (!h)
		return;
	setHand(*h);

	std::cout << "handling inventory action " << (int)pkt.actionType << " on slot " << slot << std::endl;
	std::cout << "hand has " << std::visit([](auto v) -> ItemID {
		return static_cast<ItemID>(v);
	}, getHand().first) << " x " << (int)getHand().second << std::endl;
	std::cout << "slot has " << std::visit([](auto v) -> ItemID {
		return static_cast<ItemID>(v);
	}, getItemAtSlot(slot)) << " x " << (int)getSlot(slot).second << std::endl;
	std::cout << "-----------------" << std::endl;

	ItemType typeAtSlot = getItemAtSlot(slot);
	ItemType typeAtHand = getHand().first;

	int amountAtSlot = getSlot(slot).second;
	int amountAtHand = getHand().second;

	if (pkt.actionType == InventoryActionType::INV_LEFT_CLICK)
	{
		if (amountAtHand == 0 || typeAtSlot != typeAtHand)
			swapSlots(slot, HAND_ID);
		else
		{
			mergeSlot(HAND_ID, slot);
		}
	} else if (pkt.actionType == InventoryActionType::INV_RIGHT_CLICK)
	{
		if (amountAtHand == 0)
			takeHalf(slot);
		else
		{
			if (amountAtSlot == 0 || typeAtSlot == typeAtHand)
				takeOneItemFromSlot(HAND_ID, std::optional<int>(slot));
			else
				swapSlots(slot, HAND_ID);
		}
	}

	//we make sure hand gets updated so it stays in sync with other inventories.
	if (hand.lock())
		*hand.lock() = getHand();
}

//returns the slot which has been used to insert the item. -1 in case insertion was not successful.
//if amount is still > 0, caller can just recall the function
template<int ROWS, int COLS>
int Inventory<ROWS, COLS>::insertItems(ItemType item, int &amount)
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

template<int ROWS, int COLS>
bool Inventory<ROWS, COLS>::removeItemsFromSlot(int slotNumber, itemStackSize_t amount)
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
		if (slotNumber != HAND_ID)
			freeSlots.insert(slotNumber);
	}

	return true;
}

//inserted items must have the same type as the item in the slot
template<int ROWS, int COLS>
bool Inventory<ROWS, COLS>::insertItemsToSlot(ItemType item, int slotNumber, int &amount)
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

template<int ROWS, int COLS>
bool Inventory<ROWS, COLS>::insertItemsToSlot(ItemID i, int slotNumber, int &amount)
{
	ItemType item = itemIDToItemType(i);
	return insertItemsToSlot(item, slotNumber, amount);
}

template<int ROWS, int COLS>
void Inventory<ROWS, COLS>::mergeSlot(int slotSrc, int slotDest)
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

template<int ROWS, int COLS>
void Inventory<ROWS, COLS>::takeOneItemFromSlot(int slotSrc, std::optional<int> slotDest)
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

template<int ROWS, int COLS>
void Inventory<ROWS, COLS>::takeHalf(int slotSrc)
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

template<int ROWS, int COLS>
void Inventory<ROWS, COLS>::swapSlots(int slot1, int slot2)
{
	if (slot1 >= grid.size() || slot1 < 0) return ;
	if (slot2 >= grid.size() || slot2 < 0) return ;

	auto tempSlot = getSlot(slot1);
	auto s = getSlot(slot2);

	setSlot(slot1, s.second, s.first);
	setSlot(slot2, tempSlot.second, tempSlot.first);
}

template class Inventory<4, 9>; // PlayerInventory
template class Inventory<3, 3>; // CraftingStation