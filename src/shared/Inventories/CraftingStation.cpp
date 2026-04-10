
#include "CraftingStation.hpp"

CraftingStation::CraftingStation(std::shared_ptr<InventoryExternalVariablesRefs> inventoryExternalVarsRefs) : Inventory<3, 3, 2>(inventoryExternalVarsRefs)
{
	RESULT_SLOT_ID = grid.size() - 2;
	type = InventoryType::CRAFTING_STATION;
}

bool CraftingStation::handleInventoryAction(NetInventoryAction &pkt, std::vector<PacketPtr> &pktsToSend)
{
	// ensure our local hand state is synchronized with the player's hand
    auto h = hand.lock();
    if (h)
        setHand(*h);

	ItemType typeAtSlot = getItemAtSlot(pkt.slot);
	ItemType typeAtHand = getHand().first;

	int amountAtHand = getHand().second;

	if (pkt.slot == RESULT_SLOT_ID && typeAtHand != typeAtSlot && amountAtHand != 0) return false;
	if (pkt.slot == RESULT_SLOT_ID && (pkt.actionType == InventoryActionType::INV_LEFT_CLICK || pkt.actionType == InventoryActionType::INV_RIGHT_CLICK))
	{
		craft(pktsToSend, 1);
	}
	else
		Inventory::handleInventoryAction(pkt, pktsToSend);

	ItemType prevResultSlot = grid[RESULT_SLOT_ID].first;
	checkRecipe();

	if (grid[RESULT_SLOT_ID].first != ItemType{})
		pktsToSend.push_back(createNetInventoryPkt(RESULT_SLOT_ID));

	return true;
}

void CraftingStation::setSlot(int slot, itemStackSize_t amount, ItemType type)
{
	grid[RESULT_SLOT_ID] = {};

	Inventory::setSlot(slot, amount, type);

	// craftingResultPtr = grid[RESULT_SLOT_ID].first != ItemType{} ? std::make_shared<ItemType>(grid[RESULT_SLOT_ID].first) : nullptr;
}

void CraftingStation::setSlot(int slot, itemStackSize_t amount, ItemID t)
{
	Inventory::setSlot(slot, amount, t);
}

void CraftingStation::checkRecipe()
{
	Grid g{};
	for (size_t i = 0; i < grid.size() - 2; ++i)
		g[i] = grid[i].first;

	// craftingResultPtr = nullptr;
	grid[RESULT_SLOT_ID] = { ItemType{}, 0 };

	auto shapelessKey = gridToKey(g);
	auto shapelessResult = recipes.shapelessRecipeMap.find(shapelessKey);
	if (shapelessResult != recipes.shapelessRecipeMap.end()) {
		// craftingResultPtr = std::make_shared<ItemType>(shapelessResult->second);
		grid[RESULT_SLOT_ID] = { shapelessResult->second.type, shapelessResult->second.count };
		return ;
	}

	auto result = recipes.orderedRecipeMap.find(g);
	if (result != recipes.orderedRecipeMap.end()) {
		// craftingResultPtr = std::make_shared<ItemType>(result->second);
		grid[RESULT_SLOT_ID] = { result->second.type, result->second.count };
		return ;
	}
}

bool CraftingStation::craft(std::vector<PacketPtr> &pktsToSend, itemStackSize_t amountToCraft)
{
	// if not result AND Hand != Result AND Hand not empty we return
	if (grid[RESULT_SLOT_ID].second == 0 || (grid[HAND_ID].first != grid[RESULT_SLOT_ID].first && grid[HAND_ID].second != 0))
		return false;

	// should we check the recipe to make sure it is correct ?
	// checkRecipe();

	// check if the amount of items we need to craft can fit in the hand.
	itemStackSize_t H = getHand().second;
	itemStackSize_t R = grid[RESULT_SLOT_ID].second;
	if (R == 0)
		return false;

	itemStackSize_t maxAllowed = (MAX_STACK_SIZE - H) / R;
	if (maxAllowed <= 0)
		return false;

	amountToCraft = std::min(amountToCraft, maxAllowed);

	// remove 1 layer of ingredients
	for (size_t i = 0; i < grid.size() - 2; ++i)
	{
		if (grid[i].second > 0)
		{
			removeItemsFromSlot(i, amountToCraft);
			pktsToSend.push_back(createNetInventoryPkt(i));
		}
	}

	if (amountToCraft != 1)
		grid[RESULT_SLOT_ID].second = amountToCraft * grid[RESULT_SLOT_ID].second;
	mergeSlot(RESULT_SLOT_ID, HAND_ID);

	pktsToSend.push_back(createNetInventoryPkt(HAND_ID));

	return true;
}