
#ifndef ITEM_PROP_ENTITY
#define ITEM_PROP_ENTITY

#include "blockRenderingHelperFunctions.hpp"
#include "ItemEntity.hpp"

class ItemPropEntity : public ItemEntity
{
	std::vector<float> meshVertices;
	void createMesh(std::vector<float> &meshVertices);

	public:
		ItemPropEntity(const glm::vec3 &position, float yaw, ItemType type, entityID ID);
		~ItemPropEntity();
};

#endif