
#include "ItemPropEntity.hpp"
#include "TextureManager.hpp"

ItemPropEntity::ItemPropEntity(const glm::vec3 &position, float yaw, ItemType type, entityID ID) : ItemEntity(position, yaw, type, ID)
{

}

ItemPropEntity::~ItemPropEntity()
{
}

void ItemPropEntity::createMesh(std::vector<float> &meshVertices, const glm::dvec3& eyePos, const TextureManager* texMgr)
{
	// Emit vertices in camera-relative space so the geometry stays sub-cm
	// precise at any world coordinate. The cancellation is done in double on
	// CPU; the resulting float coords are small (within view distance).
	const glm::dvec3 relD = position - eyePos;
	const float rx = static_cast<float>(relD.x);
	const float ry = static_cast<float>(relD.y);
	const float rz = static_cast<float>(relD.z);

	if (isItemFlat(type)) {
		const int layer = texMgr ? texMgr->getItemSpriteLayer(type) : 0;
		// Vegetation is rooted in the world — render as a stationary X-cross
		// matching how plants are drawn in chunks. Tools / ingots / misc
		// items have no fixed orientation, so they billboard around Y like
		// vanilla Minecraft drops.
		if (auto* b = std::get_if<BlockType>(&type)) {
			(void)b; // BlockType branch implies vegetation here (isItemFlat)
			buildItemSprite(meshVertices, rx, ry, rz, layer);
		} else {
			buildItemBillboard(meshVertices, relD, layer);
		}
		return;
	}

	if (auto* b = std::get_if<BlockType>(&type)) {
		buildCube(meshVertices, rx, ry, rz, 0, 0, *b, texMgr);
	}
}
