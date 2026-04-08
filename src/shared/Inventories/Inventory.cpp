#include "Inventory.hpp"

//MOST OF THESE FUNCTIONS WON'T WORK ONCE WE ADD OTHER TYPES OF ITEMS. FF
template<int ROWS, int COLS, int N>
Inventory<ROWS, COLS, N>::Inventory(std::shared_ptr<InventoryExternalVariablesRefs> inventoryExternalVarsRefs)
{
	for (int i = 0; i < rows * cols; i++)
		freeSlots.insert(i);

	HAND_ID = grid.size() - 1; //the last slot of the grid is reserved for the hand.
	if (inventoryExternalVarsRefs)
	{
		hand = inventoryExternalVarsRefs->hand;
		draggedSlots = inventoryExternalVarsRefs->draggedSlots;
		dragButton = inventoryExternalVarsRefs->dragButton;
	}
}

template<int ROWS, int COLS, int N>
std::pair<ItemType, itemStackSize_t> Inventory<ROWS, COLS, N>::getSlot(int slot)
{
	return grid.at(slot);
}

template<int ROWS, int COLS, int N>
ItemType Inventory<ROWS, COLS, N>::getItemAtSlot(int slot)
{
	if (slot >= grid.size() || slot < 0) return BlockType::BEGIN;
	return grid[slot].first;
}

template<int ROWS, int COLS, int N>
ItemID Inventory<ROWS, COLS, N>::getItemIDAtSlot(int slot)
{
	if (slot >= grid.size() || slot < 0) return static_cast<std::underlying_type_t<BlockType>>(BlockType::BEGIN);
	return itemTypeToItemID(grid[slot].first);
}

template<int ROWS, int COLS, int N>
void Inventory<ROWS, COLS, N>::setSlot(int slot, itemStackSize_t amount, ItemType type)
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

template<int ROWS, int COLS, int N>
void Inventory<ROWS, COLS, N>::setSlot(int slot, itemStackSize_t amount, ItemID t)
{
	auto type = itemIDToItemType(t);

	setSlot(slot, amount, type);
}

template<int ROWS, int COLS, int N>
std::unique_ptr<NetInventory> Inventory<ROWS, COLS, N>::createNetInventoryPkt(int slot)
{
	auto pkt = std::make_unique<NetInventory>();
	pkt->inventoryTypeID = static_cast<uint8_t>(type);
	pkt->type = getItemIDAtSlot(slot);
	pkt->amount = getSlot(slot).second;
	pkt->slot = slot;

	return pkt;
}

template<int ROWS, int COLS, int N>
bool Inventory<ROWS, COLS, N>::handleInventoryDrag(NetInventoryAction &pkt, std::vector<PacketPtr>& pktsToSend)
{
	if (!dragButton.lock() || !draggedSlots.lock())
		return false;

	uint8_t slot = pkt.slot;

	ItemType typeAtSlot = getItemAtSlot(slot);
	ItemType typeAtHand = getHand().first;

	int amountAtSlot = getSlot(slot).second;
	int amountAtHand = getHand().second;

	auto btn = dragButton.lock();
	auto slots = draggedSlots.lock();

	if (pkt.modifier == InventoryModifiers::INV_DRAG_BEGIN)
	{
		std::cout << "starting drag\n";
		*btn = pkt.actionType;

		//hand takes the first slot
		slots->push_back({{this->type, HAND_ID}, getSlot(HAND_ID)});

		//ONLY EMPLACE IF CAN INSERT
		if (canInsertItemsToSlot(typeAtHand, slot, amountAtHand))
			slots->push_back({{this->type, slot}, getSlot(slot)});

		return true;
	}
	else if (pkt.modifier  == InventoryModifiers::INV_DRAG_ADD)
	{
		if (canInsertItemsToSlot(typeAtHand, slot, amountAtHand))
			slots->push_back({{this->type, slot}, getSlot(slot)});

		// GLFW_MOUSE_BUTTON_LEFT = 0, GLFW_MOUSE_BUTTON_RIGHT = 1
		if (*btn == 0)
		{
			int AmountBeforeDrag = slots->front().originalValue.second;
			int amountToMove = AmountBeforeDrag / slots->size();

			for (auto& s : *slots)
			{
				if (takeFromSlotToSlot(typeAtHand, HAND_ID, s.slotIndex.slotIndex, amountToMove))
					pktsToSend.push_back(createNetInventoryPkt(s.slotIndex.slotIndex));
			}

		}
		else if (*btn == 1)
		{
			takeOneItemFromSlot(HAND_ID, slot);
			pktsToSend.push_back(createNetInventoryPkt(slot));
			pktsToSend.push_back(createNetInventoryPkt(HAND_ID));
		}
		return true;
	}

	else if (pkt.modifier == InventoryModifiers::INV_DRAG_CANCEL)
	{
		for (auto& slot : *slots)
		{
			auto pkt = std::make_unique<NetInventory>();
			pkt->inventoryTypeID = static_cast<uint8_t>(slot.slotIndex.inventoryType);
			pkt->type = itemTypeToItemID(slot.originalValue.first);
			pkt->amount = slot.originalValue.second;
			pkt->slot = slot.slotIndex.slotIndex;
			pktsToSend.push_back(std::move(pkt));

			setSlot(slot.slotIndex.slotIndex, slot.originalValue.second, slot.originalValue.first);
		}
		slots->clear();

		*btn = -1;
		return true;
	}

	//This one gotta go on Server and be applied externally
	else if (pkt.modifier  == InventoryModifiers::INV_DRAG_END)
	{
		*btn = -1;
	}

	return false;
}

template<int ROWS, int COLS, int N>
bool Inventory<ROWS, COLS, N>::handleInventoryAction(NetInventoryAction &pkt, std::vector<PacketPtr>& pktsToSend)
{
	int slot = pkt.slot;

	//we syncronize the the hand with other inventories first.
	auto h = hand.lock();
	if (!h)
		return false;
	setHand(*h);

	std::cout << (int)pkt.actionType << " " << (int)pkt.modifier << " " << slot << std::endl;
	std::cout << "----------------" << std::endl;

	if (handleInventoryDrag(pkt, pktsToSend)) return true;
	std::cout << "why\n";

	ItemType typeAtSlot = getItemAtSlot(slot);
	ItemType typeAtHand = getHand().first;

	int amountAtSlot = getSlot(slot).second;
	int amountAtHand = getHand().second;

	if (pkt.actionType == InventoryActionType::INV_LEFT_CLICK)
	{
		if (amountAtHand == 0 || typeAtSlot != typeAtHand)
			swapSlots(slot, HAND_ID);
		// else
		// {
		// 	mergeSlot(HAND_ID, slot);
		// }
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

	// fill pkts to send
	pktsToSend.push_back(createNetInventoryPkt(slot));
	pktsToSend.push_back(createNetInventoryPkt(HAND_ID));

	return true ;
}

//returns the slot which has been used to insert the item. -1 in case insertion was not successful.
//if amount is still > 0, caller can just recall the function
template<int ROWS, int COLS, int N>
int Inventory<ROWS, COLS, N>::insertItems(ItemType item, int &amount)
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

template<int ROWS, int COLS, int N>
bool Inventory<ROWS, COLS, N>::removeItemsFromSlot(int slotNumber, itemStackSize_t amount)
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

//See if it is possible to insert
template<int ROWS, int COLS, int N>
bool Inventory<ROWS, COLS, N>::canInsertItemsToSlot(ItemType item, int slotNumber, int &amount)
{
	if (slotNumber >= grid.size() || slotNumber < 0) return false;
	if (grid[slotNumber].second > 0 && grid[slotNumber].first != item) return false; //not same type

	return true;
}

//inserted items must have the same type as the item in the slot
template<int ROWS, int COLS, int N>
bool Inventory<ROWS, COLS, N>::insertItemsToSlot(ItemType item, int slotNumber, int &amount)
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

template<int ROWS, int COLS, int N>
bool Inventory<ROWS, COLS, N>::insertItemsToSlot(ItemID i, int slotNumber, int &amount)
{
	ItemType item = itemIDToItemType(i);
	return insertItemsToSlot(item, slotNumber, amount);
}

template<int ROWS, int COLS, int N>
void Inventory<ROWS, COLS, N>::mergeSlot(int slotSrc, int slotDest)
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

template<int ROWS, int COLS, int N>
bool Inventory<ROWS, COLS, N>::takeOneItemFromSlot(int slotSrc, std::optional<int> slotDest)
{
	if (slotSrc >= grid.size() || slotSrc < 0) return false;
	if (slotDest.has_value() && (*slotDest >= grid.size() || *slotDest < 0)) return false;

	auto slot = getSlot(slotSrc);
	int amount = slot.second;
	ItemType type = slot.first;
	if (amount == 0) return false;
	if (slotDest.has_value() && getSlot(*slotDest).second != 0 && getSlot(*slotDest).first != type) return false;

	bool inserted = false;
	int one = 1;
	if (slotDest.has_value())
		inserted = insertItemsToSlot(type, slotDest.value(), one);

	if (!inserted) return false;
	if (amount - 1 > 0)
		setSlot(slotSrc, amount - 1, type);
	else
		setSlot(slotSrc, 0, 0);

	return true;
}

template<int ROWS, int COLS, int N>
bool Inventory<ROWS, COLS, N>::takeFromSlotToSlot(ItemType itemType, int slotSrc, int slotDest, itemStackSize_t amount)
{
	int lamount = amount;
	if (insertItemsToSlot(itemType, slotDest, lamount))
	{
		if (removeItemsFromSlot(slotSrc, amount))
			return true;
		else //rollback. Hopefully this does not get triggered.
			removeItemsFromSlot(slotDest, amount);
	}
	return false;
}

template<int ROWS, int COLS, int N>
void Inventory<ROWS, COLS, N>::takeHalf(int slotSrc)
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

template<int ROWS, int COLS, int N>
void Inventory<ROWS, COLS, N>::swapSlots(int slot1, int slot2)
{
	if (slot1 >= grid.size() || slot1 < 0) return ;
	if (slot2 >= grid.size() || slot2 < 0) return ;

	auto tempSlot = getSlot(slot1);
	auto s = getSlot(slot2);

	setSlot(slot1, s.second, s.first);
	setSlot(slot2, tempSlot.second, tempSlot.first);
}

template class Inventory<4, 9, 1>; // PlayerInventory
template class Inventory<3, 3, 2>; // CraftingStation