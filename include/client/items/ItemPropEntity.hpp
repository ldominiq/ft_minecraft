
#ifndef ITEM_PROP_ENTITY
#define ITEM_PROP_ENTITY

#include "blockRenderingHelperFunctions.hpp"
#include "ItemEntity.hpp"

class TextureManager; // Forward declaration

class ItemPropEntity : public ItemEntity
{
	std::vector<float> meshVertices;

	public:
		void createMesh(std::vector<float> &meshVertices, const glm::dvec3& eyePos, const TextureManager* texMgr = nullptr, float skyFactor = 1.0f, float blockFactor = 0.0f) override;

		ItemPropEntity(const glm::vec3 &position, float yaw, ItemType type, entityID ID);
		~ItemPropEntity();
};

#endif