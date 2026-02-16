
#ifndef ITEM_ENTITY_HPP
#define ITEM_ENTITY_HPP

#include "Entity.hpp"
#include "Item.hpp"

class ItemEntity : public Entity
{
	int32_t spawnTick = 0; //Server tick the prop spawned at.

	protected:
		ItemType type;

	public:
		glm::vec3 getDesiredMove() override;

		inline EEntityTypes getEntityType() const override { return EEntityTypes::ITEMS; }
		ItemType inline getItemType() const { return type; }
		ItemID getItemID() const {
			return std::visit([](auto& value) -> ItemID {
				return static_cast<ItemID>(value);
			}, type);
		}

		inline const int getSpawnTick() const {return spawnTick;}

		ItemEntity(const glm::vec3 &position, float yaw, ItemType type, int32_t spawnTick, bool isLaunched = false);
		ItemEntity(const glm::vec3 &position, float yaw, ItemType type, entityID ID);
		virtual ~ItemEntity();
};

#endif