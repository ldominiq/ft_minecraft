
#include "CraftingStation.hpp"

CraftingStation::CraftingStation(std::shared_ptr<std::pair<ItemType, itemStackSize_t>> handPtr) : Inventory<3, 3>(handPtr)
{
}