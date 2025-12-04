
#include "ItemPropEntity.hpp"

ItemPropEntity::ItemPropEntity(const glm::vec3 &position, float yaw, BlockType type, entityID ID) : ItemEntity(position, yaw, type, ID)
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
