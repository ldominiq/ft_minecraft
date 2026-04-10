
#ifndef IINVENTORY_HPP
#define IINVENTORY_HPP

#include "Protocol.hpp"
#include "Item.hpp"

using itemStackSize_t = uint8_t;

class IInventory {

public:
    virtual ~IInventory() = default;

	bool hasDraggedSlots = false;
	virtual bool handleInventoryAction(NetInventoryAction &action, std::vector<PacketPtr> &pktsToSend) = 0;

	uint8_t HAND_ID = 0; //the last slot of the grid is reserved for the hand.
	uint8_t getHandID() const { return HAND_ID; }
	virtual void setSlot(int slot, itemStackSize_t amount, ItemType type) = 0;
};

#endif // IINVENTORY_HPP
