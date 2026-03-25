
#include "blockRenderingHelperFunctions.hpp"
#include "TextureManager.hpp"

void buildCube(
	std::vector<float>& meshVertices,
	float x, float y, float z,
	int originX, int originZ,
	BlockType type,
	const TextureManager* texMgr,
	bool isIlluminated)
{
	addFace(meshVertices, x, y, z, originX, originZ, type, 0, texMgr, isIlluminated);
	addFace(meshVertices, x, y, z, originX, originZ, type, 1, texMgr, isIlluminated);
	addFace(meshVertices, x, y, z, originX, originZ, type, 2, texMgr, isIlluminated);
	addFace(meshVertices, x, y, z, originX, originZ, type, 3, texMgr, isIlluminated);
	addFace(meshVertices, x, y, z, originX, originZ, type, 4, texMgr, isIlluminated);
	addFace(meshVertices, x, y, z, originX, originZ, type, 5, texMgr, isIlluminated);
}

void build2DInventoryCube(
	std::vector<float>& meshVertices,
	glm::vec2 origin,
	float scale,
	BlockType type,
	const TextureManager* texMgr)
{
	addInventoryFace(meshVertices, origin, scale, type, 2, texMgr);
	addInventoryFace(meshVertices, origin, scale, type, 1, texMgr);
	addInventoryFace(meshVertices, origin, scale, type, 0, texMgr);
}

void addInventoryFace(
	std::vector<float>& meshVertices,
	glm::vec2 origin,				// inventory slot position
	float scale,
	BlockType type,
	int face,
	const TextureManager* texMgr)
{
    constexpr int quadToTri[6] = { 0,1,2, 2,3,0 };

    // Map inventory face index to block face index for texture lookup:
    // inventory face 0 = BOTTOM LEFT = block face 5 (LEFT/-X)
    // inventory face 1 = BOTTOM RIGHT = block face 4 (RIGHT/+X)
    // inventory face 2 = TOP = block face 2 (TOP/+Y)
    static const int invFaceToBlockFace[3] = { 5, 4, 2 };
    int blockFace = invFaceToBlockFace[face];

    float texLayer = 0.0f;
    if (texMgr) {
        const BlockTextures& bt = texMgr->getBlockTextures(type);
        texLayer = static_cast<float>(bt.getLayerForFace(blockFace));
    }

    for (int i = 0; i < 6; ++i)
    {
        int v = quadToTri[i];

        glm::vec2 basePos = unitFacePositionsInventory[face][v];
        glm::vec2 pos = origin + basePos * scale;

        glm::vec2 uv = {
            uvTemplate[i].x,
            uvTemplate[i].y
        };

        meshVertices.push_back(pos.x);
        meshVertices.push_back(pos.y);
        meshVertices.push_back(uv.x);
        meshVertices.push_back(uv.y);
        meshVertices.push_back(texLayer);
    }
}

void addFace(
	std::vector<float>& meshVertices,
    float x, float y, float z,
    int originX, int originZ,
    BlockType type, int face,
    const TextureManager* texMgr,
    bool isIlluminated)
{
    glm::vec3 normal = faceNormals[face];

    // Get texture layer for this block face from TextureManager
    float texLayer = 0.0f;
    if (texMgr) {
        const BlockTextures& bt = texMgr->getBlockTextures(type);
        texLayer = static_cast<float>(bt.getLayerForFace(face));
    }
    
    for (int i = 0; i < 6; ++i) {
        glm::vec3 basePos = unitFacePositions[face][i];

        // world aligned [0..1] + block origin
        glm::vec3 pos = { originX + x + basePos.x,
                    y + basePos.y,
                    originZ + z + basePos.z };
        if (!isIlluminated) //if it's itemprop..
		{
			constexpr float scale = 0.2f;

			// Translate vertex so (0.5, 0.5, 0.5) becomes the origin, scale, then translate back
			pos = glm::vec3(originX + x, y, originZ + z)
				+ glm::vec3(basePos - 0.5f) * scale
				+ glm::vec3(0.0f, 0.5f * scale, 0.0f);
		}

        glm::vec2 uv = {
            uvTemplate[i].x,
            uvTemplate[i].y
        };

        // Pack vertex attributes
        meshVertices.push_back(pos.x);
        meshVertices.push_back(pos.y);
        meshVertices.push_back(pos.z);
        meshVertices.push_back(uv.x);
        meshVertices.push_back(uv.y);
        meshVertices.push_back(texLayer);

        if (isIlluminated) {
            meshVertices.push_back(pos.y);     // gradient Y
            meshVertices.push_back(normal.x);
            meshVertices.push_back(normal.y);
            meshVertices.push_back(normal.z);
        }
    }
}
