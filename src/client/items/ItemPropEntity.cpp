
#include "ItemPropEntity.hpp"

ItemPropEntity::ItemPropEntity(const glm::vec3 &position, float yaw, ItemType type, entityID ID) : ItemEntity(position, yaw, type, ID)
{
	nextPosition = position;
}

ItemPropEntity::~ItemPropEntity()
{
}

void ItemPropEntity::createMesh(std::vector<float> &meshVertices, const TextureManager* texMgr)
{
	std::visit([&](auto& value) {
		using T = std::decay_t<decltype(value)>;
		if constexpr (std::is_same_v<T, BlockType>) {
			buildCube(meshVertices, position.x, position.y, position.z, 0, 0, value, texMgr);
		} else if constexpr (std::is_same_v<T, WeaponType>) {
			// handle WeaponType
		} else {
			// handle MiscType
		}
	}, type);
}
