
#ifndef CRAFTING_STATION_HPP
#define CRAFTING_STATION_HPP

#include "Inventory.hpp"
#include "Crafts.hpp"

class CraftingStation : public Inventory<3, 3, 2>
{
	int RESULT_SLOT_ID = 0;

	Recipes recipes;

	void checkRecipe();

	public:
		CraftingStation(std::shared_ptr<InventoryExternalVariablesRefs> inventoryExternalVarsRefs);
		~CraftingStation() override = default;

		bool handleInventoryAction(NetInventoryAction &pkt, std::vector<PacketPtr> &pktsToSend) override;
		void setSlot(int slot, itemStackSize_t amount, ItemType type) override;
		void setSlot(int slot, itemStackSize_t amount, ItemID t) override;

		int getResultSlotID() const { return RESULT_SLOT_ID; }

		//returns whether it successfully crafted or not.
		bool craft(std::vector<PacketPtr> &pktsToSend, itemStackSize_t amountToCraft = 1);
};

#endif