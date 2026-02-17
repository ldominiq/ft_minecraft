#ifndef INVENTORY_HPP
#define INVENTORY_HPP

#include <vector>
#include <set>
#include <unordered_map>
#include <map>
#include <iostream>

#include "Item.hpp"

constexpr int16_t INVALID_SLOT = -1;

class Inventory {

	protected:
		static const int rows = 4;
		static const int cols = 9;
	
	private:
		static const uint8_t MAX_STACK_SIZE = UINT8_MAX;

		std::array<std::pair<ItemType, uint8_t>, rows * cols> grid{};	//inventory grid. should default initialize to every value {BlockType::Begin, 0}.
		std::multimap<ItemType, uint8_t> itemsIndexes; //maps items to where they are in the inventory
		std::set<int> freeSlots;	//keep track of free slots

		std::pair<ItemType, uint8_t> hand;	//Items that the player is currently holding (in the menu. hovering over the inventory);

	public:
		Inventory();
		~Inventory() = default;

		uint8_t activeHotbarSlot = 0; // 0 to 35?

		std::pair<ItemType, uint8_t> getSlot(uint8_t slot);
		ItemType getItemAtSlot(int slot);
		ItemType getActiveItem();
		ItemID getActiveItemID();

		int insertItems(ItemType, int amount);
		bool removeItemsFromSlot(int slotNumber, int amount);
		bool insertItemsToSlot(ItemType item, int slotNumber, int amount);

		inline int getFirstFreeSlot() const {
			if (freeSlots.empty())
				return -1;
			return *freeSlots.begin();
		}
};

#endif