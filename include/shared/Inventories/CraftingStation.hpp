
#ifndef CRAFTING_STATION_HPP
#define CRAFTING_STATION_HPP

#include "Inventory.hpp"

class CraftingStation : public Inventory<3, 3>
{

	public:
		CraftingStation(std::shared_ptr<std::pair<ItemType, itemStackSize_t>> handPtr = nullptr);
		~CraftingStation() override = default;

};

#endif