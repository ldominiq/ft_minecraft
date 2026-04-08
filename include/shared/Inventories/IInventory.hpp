
#ifndef IINVENTORY_HPP
#define IINVENTORY_HPP

#include "Protocol.hpp"

class IInventory {
public:
    virtual ~IInventory() = default;

	virtual bool handleInventoryAction(NetInventoryAction &action, std::vector<PacketPtr> &pktsToSend) = 0;

	uint8_t HAND_ID = 0; //the last slot of the grid is reserved for the hand.
	uint8_t getHandID() const { return HAND_ID; }
};

#endif // IINVENTORY_HPP
