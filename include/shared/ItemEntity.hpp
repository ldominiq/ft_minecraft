
#ifndef ITEM_ENTITY_HPP
#define ITEM_ENTITY_HPP

#include "Entity.hpp"
#include "Item.hpp"

class ItemEntity : public Entity
{
	protected:
		BlockType type;

	public:
		glm::vec3 getDesiredMove() override;

		inline EEntityTypes getEntityType() const override { return EEntityTypes::ITEMS; }
		BlockType inline getItemType() const { return type; }

		ItemEntity(glm::vec3 &position, float yaw, BlockType type, bool isLaunched = false);
		ItemEntity(glm::vec3 &position, float yaw, BlockType type, entityID ID);
		virtual ~ItemEntity();
};

#endif