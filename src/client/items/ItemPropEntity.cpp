
#include "ItemPropEntity.hpp"

ItemPropEntity::ItemPropEntity(const glm::vec3 &position, float yaw, ItemType type, entityID ID) : ItemEntity(position, yaw, type, ID)
{

}

ItemPropEntity::~ItemPropEntity()
{
}

void ItemPropEntity::createMesh(std::vector<float> &meshVertices, const glm::dvec3& eyePos, const TextureManager* texMgr)
{
	// Emit vertices in camera-relative space so the cube stays sub-cm precise
	// at any world coordinate. The cancellation is done in double on CPU; the
	// resulting float coords are small (within view distance of the eye).
	const glm::dvec3 relD = position - eyePos;
	const float rx = static_cast<float>(relD.x);
	const float ry = static_cast<float>(relD.y);
	const float rz = static_cast<float>(relD.z);

	std::visit([&](auto& value) {
		using T = std::decay_t<decltype(value)>;
		if constexpr (std::is_same_v<T, BlockType>) {
			buildCube(meshVertices, rx, ry, rz, 0, 0, value, texMgr);
		} else if constexpr (std::is_same_v<T, WeaponType>) {
			// handle WeaponType
		} else {
			// handle MiscType
		}
	}, type);
}
