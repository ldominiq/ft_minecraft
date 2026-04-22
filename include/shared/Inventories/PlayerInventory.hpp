
#ifndef PLAYER_INVENTORY_HPP
#define PLAYER_INVENTORY_HPP

#include "Inventory.hpp"

class PlayerInventory : public Inventory<4, 9, 1> {

	public:
		PlayerInventory(std::shared_ptr<InventoryExternalVariablesRefs> inventoryExternalVarsRefs = nullptr);
		~PlayerInventory() override = default;

		ItemType getActiveItem();
		ItemID getActiveItemID();

		uint8_t activeHotbarSlot = 0; // 0 to 35?
};

#endif