#ifndef INVENTORY_HPP
#define INVENTORY_HPP

#include <vector>
#include <set>
#include <unordered_map>
#include <map>
#include <iostream>
#include <optional>

#include "Item.hpp"

//careaful, there might be a confusion with (uint)-1 used for when activeHotbarSlot isn't set
constexpr int16_t INVALID_SLOT = -1;
constexpr uint8_t HAND_ID = 36;
using itemStackSize_t = uint8_t;

class Inventory {

	protected:
		static const int rows = 4;
		static const int cols = 9;
	
	private:
		static const itemStackSize_t MAX_STACK_SIZE = UINT8_MAX;

		std::array<std::pair<ItemType, itemStackSize_t>, rows * cols + 1> grid{};	//inventory grid. should default initialize to every value {BlockType::Begin, 0}.
		std::multimap<ItemType, int> itemsIndexes; //maps items to where they are in the inventory
		std::set<int> freeSlots;	//keep track of free slots

	public:
		Inventory();
		virtual ~Inventory() = default;

		std::pair<ItemType, itemStackSize_t> getHand() {return grid[HAND_ID];}
		uint8_t activeHotbarSlot = 0; // 0 to 35?

		std::pair<ItemType, itemStackSize_t> getSlot(int slot);
		ItemType getItemAtSlot(int slot);
		ItemID getItemIDAtSlot(int slot);
		ItemType getActiveItem();
		ItemID getActiveItemID();

		void setSlot(int slot, itemStackSize_t amount, ItemType type);
		void setSlot(int slot, itemStackSize_t amount, ItemID type);

		int insertItems(ItemType type, int &amount);

		bool insertItemsToSlot(ItemType item, int slotNumber, int &amount);
		bool insertItemsToSlot(ItemID item, int slotNumber, int &amount);

		bool removeItemsFromSlot(int slotNumber, itemStackSize_t amount);

		void mergeSlot(int slotSrc, int slotDest);
		void takeOneItemFromSlot(int slotSrc, std::optional<int> slotDest = std::nullopt);
		//will always takes TO hand
		void takeHalf(int slotSrc);
		void swapSlots(int slot1, int slot2);

		inline int getFirstFreeSlot() const {
			if (freeSlots.empty())
				return -1;
			return *freeSlots.begin();
		}
};

#endif