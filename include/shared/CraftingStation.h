
#ifndef CRAFTING_STATION_HPP
#define CRAFTING_STATION_HPP

#include "Inventory.hpp"

class CraftingStation : public Inventory
{
	rows = 3;
	cols = 3;

	public:
		CraftingStation();

		void setPosition(float x, float y);
		void setSize(float width, float height);

};

#endif