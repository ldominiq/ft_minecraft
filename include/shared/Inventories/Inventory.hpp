#ifndef INVENTORY_HPP
#define INVENTORY_HPP

#include <vector>
#include <set>
#include <unordered_map>
#include <map>
#include <iostream>
#include <optional>

#include "Item.hpp"
#include "IInventory.hpp"

enum class InventoryType : uint8_t {
	NONE,
	PLAYER,
	CRAFTING_STATION,
	CHEST, //unused
};

//careaful, there might be a confusion with (uint)-1 used for when activeHotbarSlot isn't set
constexpr int16_t INVALID_SLOT = -1;
using itemStackSize_t = uint8_t;

struct SlotIndexInfo {
	InventoryType inventoryType;
	uint8_t slotIndex;
};

struct draggedSlotInfo {
	SlotIndexInfo slotIndex;
	std::pair<ItemType, itemStackSize_t> originalValue;
};

struct InventoryExternalVariablesRefs {
	std::shared_ptr<std::pair<ItemType, itemStackSize_t>> hand = std::make_shared<std::pair<ItemType, itemStackSize_t>>();
	std::shared_ptr<std::vector<draggedSlotInfo>> draggedSlots = std::make_shared<std::vector<draggedSlotInfo>>();
	std::shared_ptr<int8_t> dragButton = std::make_shared<int8_t>(-1);
};

//N represents extra slots. Like hand or result for crafting station.
template<int ROWS, int COLS, int N>
class Inventory : public IInventory{

	protected:
		static const int rows = ROWS;
		static const int cols = COLS;
		static const int n = N;

		static const itemStackSize_t MAX_STACK_SIZE = UINT8_MAX;

		InventoryType type = InventoryType::NONE;

		//take a shared/weak ptr of hand so it can be shared between inventories
		std::weak_ptr<std::pair<ItemType, itemStackSize_t>> hand;
		std::weak_ptr<std::vector<draggedSlotInfo>> draggedSlots; //first slot is taken by HAND_ID. Which corresponds to the original slot that must be subdivided.
		std::weak_ptr<int8_t> dragButton;

		std::array<std::pair<ItemType, itemStackSize_t>, rows * cols + N> grid{};	//inventory grid. should default initialize to every value {BlockType::Begin, 0}. +1 cause for some reason the hand is also in there

		std::multimap<ItemType, int> itemsIndexes; //maps items to where they are in the inventory
		std::set<int> freeSlots;	//keep track of free slots

	public:

		Inventory(std::shared_ptr<InventoryExternalVariablesRefs> inventoryExternalVarsRefs);
		virtual ~Inventory() = default;

		std::pair<ItemType, itemStackSize_t> getHand() {return grid[HAND_ID];}
		void setHand(std::pair<ItemType, itemStackSize_t> handItem) {grid[HAND_ID] = handItem;}

		std::pair<ItemType, itemStackSize_t> getSlot(int slot);
		ItemType getItemAtSlot(int slot);
		ItemID getItemIDAtSlot(int slot);

		virtual void setSlot(int slot, itemStackSize_t amount, ItemType type);
		virtual void setSlot(int slot, itemStackSize_t amount, ItemID type);

		std::unique_ptr<NetInventory> createNetInventoryPkt(int slot);
		//return wether there's any packet to send or not.
		bool handleInventoryDrag(NetInventoryAction &pkt, std::vector<PacketPtr>& pktsToSend);
		virtual bool handleInventoryAction(NetInventoryAction &pkt, std::vector<PacketPtr>& pkts);

		int insertItems(ItemType type, int &amount);

		bool canInsertItemsToSlot(ItemType item, int slotNumber, int &amount);
		bool insertItemsToSlot(ItemType item, int slotNumber, int &amount);
		bool insertItemsToSlot(ItemID item, int slotNumber, int &amount);

		bool removeItemsFromSlot(int slotNumber, itemStackSize_t amount);

		//Src - Dst
		void mergeSlot(int slotSrc, int slotDest);
		bool takeOneItemFromSlot(int slotSrc, std::optional<int> slotDest = std::nullopt);
		bool takeFromSlotToSlot(ItemType itemType, int slotSrc, int slotDest, itemStackSize_t amount);
		//will always takes TO hand
		void takeHalf(int slotSrc);
		void swapSlots(int slot1, int slot2);

		int getRows() const { return rows; }
		int getCols() const { return cols; }

		inline int getFirstFreeSlot() const {
			if (freeSlots.empty())
				return -1;
			return *freeSlots.begin();
		}
};

#endif