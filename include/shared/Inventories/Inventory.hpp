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

template<int ROWS, int COLS>
class Inventory : public IInventory{

	protected:
		static const int rows = ROWS;
		static const int cols = COLS;

		static const itemStackSize_t MAX_STACK_SIZE = UINT8_MAX;

		//take a shared/weak ptr of hand so it can be shared between inventories
		std::weak_ptr<std::pair<ItemType, itemStackSize_t>> hand;

		std::array<std::pair<ItemType, itemStackSize_t>, rows * cols + 1> grid{};	//inventory grid. should default initialize to every value {BlockType::Begin, 0}.

		std::multimap<ItemType, int> itemsIndexes; //maps items to where they are in the inventory
		std::set<int> freeSlots;	//keep track of free slots

	public:

		Inventory(std::shared_ptr<std::pair<ItemType, itemStackSize_t>> handPtr = nullptr);
		virtual ~Inventory() = default;

		std::pair<ItemType, itemStackSize_t> getHand() {return grid[HAND_ID];}
		void setHand(std::pair<ItemType, itemStackSize_t> handItem) {grid[HAND_ID] = handItem;}

		std::pair<ItemType, itemStackSize_t> getSlot(int slot);
		ItemType getItemAtSlot(int slot);
		ItemID getItemIDAtSlot(int slot);

		void setSlot(int slot, itemStackSize_t amount, ItemType type);
		void setSlot(int slot, itemStackSize_t amount, ItemID type);

		void handleInventoryAction(NetInventoryAction &pkt);

		int insertItems(ItemType type, int &amount);

		bool insertItemsToSlot(ItemType item, int slotNumber, int &amount);
		bool insertItemsToSlot(ItemID item, int slotNumber, int &amount);

		bool removeItemsFromSlot(int slotNumber, itemStackSize_t amount);

		void mergeSlot(int slotSrc, int slotDest);
		void takeOneItemFromSlot(int slotSrc, std::optional<int> slotDest = std::nullopt);
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