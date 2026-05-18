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
	if (std::cmp_greater_equal(slot, grid.size()) || slot < 0) return {BlockType::BEGIN, 0};
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
		// The cursor (HAND_ID) is synced via raw setHand()/grid writes that
		// bypass index maintenance, so it must never enter itemsIndexes or it
		// leaves a permanently stale entry.
		if (slot != HAND_ID)
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
void Inventory<ROWS, COLS, N>::createFullInventoryPkt(std::vector<PacketPtr>& pktsToSend)
{
	for (int i = 0; i < grid.size(); i++)
	{
		auto pkt = createNetInventoryPkt(i);
		pktsToSend.push_back(std::move(pkt));
	}
}

template<int ROWS, int COLS, int N>
void Inventory<ROWS, COLS, N>::addDraggedSlot(NetInventoryAction &pkt)
{
	uint8_t slot = pkt.slot;
	if (slot >= rows * cols) return; //patch solution to exclude result crafting slot & possibly hand too

	ItemType typeAtSlot = getItemAtSlot(slot);
	ItemType typeAtHand = getHand().first;

	int amountAtSlot = getSlot(slot).second;
	int amountAtHand = getHand().second;

	auto btn = dragButton.lock();
	auto slots = draggedSlots.lock();

	if (static_cast<InventoryType>(pkt.inventoryTypeID) == this->type)
	{
		bool alreadyInList = false;
		for(auto & s : *slots)
		{
			if (s.slotIndex.slotIndex == slot && s.slotIndex.inventoryType == this->type)
				alreadyInList = true;
		}

		if (!alreadyInList && canInsertItemsToSlot(typeAtHand, slot, amountAtHand))
		{
			slots->push_back({{this->type, slot}, getSlot(slot)});
			hasDraggedSlots = true;
		}
	}
}

template<int ROWS, int COLS, int N>
bool Inventory<ROWS, COLS, N>::handleDragModifier(NetInventoryAction &pkt, std::vector<PacketPtr>& pktsToSend)
{
	auto h = hand.lock();
	if (!h)
		return false;
	setHand(*h);

	uint8_t slot = pkt.slot;

	ItemType typeAtSlot = getItemAtSlot(slot);
	ItemType typeAtHand = getHand().first;

	int amountAtSlot = getSlot(slot).second;
	int amountAtHand = getHand().second;

	auto btn = dragButton.lock();
	auto slots = draggedSlots.lock();

	if (slots->size() < 2) return false;

	// GLFW_MOUSE_BUTTON_LEFT = 0, GLFW_MOUSE_BUTTON_RIGHT = 1
	if (*btn == 0)
	{
		int amountBeforeDrag = slots->front().originalValue.second;
		int amountPerSlot = amountBeforeDrag / (slots->size() - 1);
		int amountAfterDrag = getHand().second;

		for (auto& s : *slots)
		{
			if (s.slotIndex.slotIndex == HAND_ID || s.slotIndex.inventoryType != this->type) continue;
			int amountToMove = amountPerSlot;
			if (takeFromSlotToSlot(typeAtHand, HAND_ID, s.slotIndex.slotIndex, amountToMove))
				pktsToSend.push_back(createNetInventoryPkt(s.slotIndex.slotIndex));
			amountAfterDrag -= amountPerSlot + amountToMove;
		}

		setSlot(HAND_ID, amountAfterDrag, typeAtHand);
		// pktsToSend.push_back(createNetInventoryPkt(HAND_ID));
	}
	else if (*btn == 1)
	{
		int amountBeforeDrag = slots->front().originalValue.second;
		int amountAfterDrag = getHand().second;
		for (auto& s : *slots)
		{
			if (amountAfterDrag == 0) break;
			if (s.slotIndex.slotIndex == HAND_ID || s.slotIndex.inventoryType != this->type) continue;
			if (takeOneItemFromSlot(HAND_ID, s.slotIndex.slotIndex))
			{
				pktsToSend.push_back(createNetInventoryPkt(s.slotIndex.slotIndex));
				amountAfterDrag--;
			}
		}
		setSlot(HAND_ID, amountAfterDrag, typeAtHand);
		// pktsToSend.push_back(createNetInventoryPkt(HAND_ID));
	}

	if (hand.lock())
		*hand.lock() = getHand();

	return true;

}

template<int ROWS, int COLS, int N>
bool Inventory<ROWS, COLS, N>::handleInventoryModifiers(NetInventoryAction &pkt, std::vector<PacketPtr>& pktsToSend)
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

	if (pkt.modifier == InventoryModifiers::INV_DOUBLE_CLICK)
	{
		ItemType typeToStack = amountAtHand > 0 ? typeAtHand : typeAtSlot;
		auto itemSlots = itemsIndexes.equal_range(typeToStack);

		// snapshot indices first because mergeSlot() mutates itemsIndexes
		std::vector<int> indices;
		for (auto it = itemSlots.first; it != itemSlots.second; ++it)
		{
			// skip the slot that was clicked and the hand slot
			if (it->second == HAND_ID) continue;
			indices.push_back(it->second);
		}

		for (int idx : indices)
		{
			if (grid[HAND_ID].second == MAX_STACK_SIZE) break;
			mergeSlot(idx, HAND_ID);
			pktsToSend.push_back(createNetInventoryPkt(idx));
		}

		pktsToSend.push_back(createNetInventoryPkt(HAND_ID));
        return true;
	}

	if (pkt.modifier == InventoryModifiers::INV_DRAG_BEGIN)
	{
		*btn = pkt.actionType;

		//hand takes the first slot
		slots->push_back({{this->type, HAND_ID}, getSlot(HAND_ID)});

		//ONLY EMPLACE IF CAN INSERT
		if (canInsertItemsToSlot(typeAtHand, slot, amountAtHand))
			slots->push_back({{this->type, slot}, getSlot(slot)});

		hasDraggedSlots = true;

		return true;
	}
	else if (pkt.modifier  == InventoryModifiers::INV_DRAG_END)
	{
		if (slots->size() == 1 || slots->size() == 2)
		{
			//opposite of what isfound in handleInventoryAction. kept the ";" to make it clearer
			if (pkt.actionType == InventoryActionType::INV_LEFT_CLICK)
			{
				if (amountAtHand == 0)
					;
				else
				{
					if (typeAtHand == typeAtSlot)
						mergeSlot(HAND_ID, slot);
					else
						swapSlots(slot, HAND_ID);
				}
			} else if (pkt.actionType == InventoryActionType::INV_RIGHT_CLICK)
			{
				if (amountAtHand == 0)
					;
				else
				{
					if (amountAtSlot == 0 || typeAtSlot == typeAtHand)
						takeOneItemFromSlot(HAND_ID, std::optional<int>(slot));
					else
						swapSlots(slot, HAND_ID);
				}
			}
		}

		pktsToSend.push_back(createNetInventoryPkt(slot));
		pktsToSend.push_back(createNetInventoryPkt(HAND_ID));

		slots->clear();
		hasDraggedSlots = false;
		*btn = -1;
		return true;
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

	if (handleInventoryModifiers(pkt, pktsToSend))
	{
		if (hand.lock())
			*hand.lock() = getHand();
		return true;
	}

	ItemType typeAtSlot = getItemAtSlot(slot);
	ItemType typeAtHand = getHand().first;

	int amountAtSlot = getSlot(slot).second;
	int amountAtHand = getHand().second;

	if (pkt.actionType == InventoryActionType::INV_LEFT_CLICK)
	{
		if (amountAtHand == 0)
			swapSlots(slot, HAND_ID);
		else
			;
	} else if (pkt.actionType == InventoryActionType::INV_RIGHT_CLICK)
	{
		if (amountAtHand == 0)
			takeHalf(slot);
		else
		{
			;
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
		// Never stack pickups onto the cursor, and skip stale index entries
		// whose slot no longer actually holds this item: blindly bumping the
		// count would leave a slot with amount > 0 but a "nothing" type.
		if (itemSlot->second == HAND_ID
			|| grid[itemSlot->second].first != item
			|| grid[itemSlot->second].second == 0)
			continue;

		auto& stack = grid[itemSlot->second].second;

		if (stack >= MAX_STACK_SIZE)
			continue;

		int inserted = std::min<int>(MAX_STACK_SIZE - stack, amount);

		stack += inserted;
		amount -= inserted;

		if (hand.lock())
			*hand.lock() = getHand();

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

	if (hand.lock())
		*hand.lock() = getHand();

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
	if (amount == 0 ) return false;
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

	if (hand.lock())
		*hand.lock() = getHand();
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
bool Inventory<ROWS, COLS, N>::takeFromSlotToSlot(ItemType itemType, int slotSrc, int slotDest, int &amount)
{
 	int requested = amount;
 	int remaining = amount;
 	if (insertItemsToSlot(itemType, slotDest, remaining))
 	{
 		int inserted = requested - remaining;
 		amount = remaining;
 		if (removeItemsFromSlot(slotSrc, inserted))
 			return true;
 		else //rollback. Hopefully this does not get triggered.
 			removeItemsFromSlot(slotDest, inserted);
 	}
 	else
 		amount = remaining;

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

template<int ROWS, int COLS, int N>
void Inventory<ROWS, COLS, N>::saveToStream(std::ofstream& out) const
{
    if (!out) {
        std::cerr << "Error opening file for writing while saving inventory to stream" << std::endl;
        return;
    }

    out.write(reinterpret_cast<const char*>(grid.data()),
              sizeof(std::pair<ItemType, itemStackSize_t>) * grid.size());
}

template<int ROWS, int COLS, int N>
void Inventory<ROWS, COLS, N>::loadFromStream(std::ifstream& in)
{
    if (!in) {
        std::cerr << "Error opening file for reading while loading inventory from stream" << std::endl;
        return;
    }

    in.read(reinterpret_cast<char*>(grid.data()),
            sizeof(std::pair<ItemType, itemStackSize_t>) * grid.size());

    // The grid was overwritten in place, so the freeSlots/itemsIndexes
    // bookkeeping seeded by the constructor no longer matches what was
    // loaded. Rebuild it from the actual contents, otherwise getFirstFreeSlot()
    // still reports every slot as free and pickups overwrite existing items.
    freeSlots.clear();
    itemsIndexes.clear();
    for (int i = 0; i < rows * cols; ++i)
    {
        if (grid[i].second == 0)
        {
            grid[i] = {};
            freeSlots.insert(i);
        }
        else
            itemsIndexes.insert({ grid[i].first, i });
    }
}

template class Inventory<4, 9, 1>; // PlayerInventory
template class Inventory<3, 3, 2>; // CraftingStation