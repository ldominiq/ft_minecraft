
#include "ItemPropEntity.hpp"

ItemPropEntity::ItemPropEntity(glm::vec3 position, BlockType type, entityID ID) : ItemEntity(position, type, ID)
{
	nextPosition = position;
}

ItemPropEntity::~ItemPropEntity()
{
}

void ItemPropEntity::createMesh(std::vector<float> &meshVertices)
{
	buildCube(meshVertices, position.x, position.y, position.z, 0, 0, type);
}
