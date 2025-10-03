
#ifndef ITEM_ENTITY_HPP
#define ITEM_ENTITY_HPP

#include "Entity.hpp"
#include "Item.hpp"

class ItemEntity : public Entity
{
	// static ItemEntityIDManager idManager;

	protected:
		BlockType item;

	public:
		glm::vec3 getDesiredMove() override;

		inline EEntityTypes getEntityType() const override { return EEntityTypes::ITEMS; }
		BlockType inline getItemType() const { return item; }

		ItemEntity(glm::vec3 position, float yaw, BlockType item);
		ItemEntity(glm::vec3 position, BlockType item, entityID entityID);
		virtual ~ItemEntity();
};

#endif