
#ifndef ITEM_ENTITY_HPP
#define ITEM_ENTITY_HPP

#include "Entity.hpp"
#include "Item.hpp"

class ItemEntity : public Entity
{
	ItemID item;
	static ItemEntityIDManager idManager;

	public:
		glm::vec3 getDesiredMove() override;

		inline EEntityTypes getEntityType() const override { return EEntityTypes::ITEMS; }
		ItemID inline getItemType() const { return item; }

		ItemEntity(glm::vec3 position, float yaw, ItemID item);
		ItemEntity(glm::vec3 position, float yaw, ItemID ID, uint32_t entityID);
		virtual ~ItemEntity();
};

#endif