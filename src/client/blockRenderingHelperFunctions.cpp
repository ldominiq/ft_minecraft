
#include "blockRenderingHelperFunctions.hpp"

// {col, row} for each BlockType, indexed by (int)BlockType
// Multi-face blocks (GRASS, LOG) are handled in the switch below
static constexpr glm::ivec2 atlasMap[] = {
    {0, 0}, // BEGIN (unused)
    {0, 0}, // AIR   (unused)
    {1, 0}, // GRASS (default: side, overridden per-face below)
    {2, 0}, // DIRT
    {3, 0}, // STONE
    {4, 0}, // SAND
    {5, 0}, // SNOW
    {6, 0}, // WATER
    {7, 0}, // BEDROCK
    {8, 0}, // LOG   (default: bark, overridden per-face below)
    {9, 0}, // LEAVES
    {0, 1}, // IRON
    {1, 1}, // GOLD
    {2, 1}, // DIAMOND
    {3, 1}, // URANIUM
};


glm::vec2 getTextureOffset(BlockType type, int face) {
	int idx = static_cast<int>(type);
    int col = atlasMap[idx].x;
    int row = atlasMap[idx].y;


    switch (type) {
        case BlockType::GRASS:
            if (face == 2)      { col = 0; row = 0; } // top
            else if (face == 3) { col = 2; row = 0; } // bottom = dirt
            break; // sides: use atlasMap default {1, 0}

        case BlockType::LOG:
            if (face == 2 || face == 3) { col = 8; row = 1; } // top/bottom = rings
            break; // sides: use atlasMap default {8, 0}

        default:
            break;
    }

    return glm::vec2(col, row);
}

void buildCube(
	std::vector<float>& meshVertices,
	float x, float y, float z,
	int originX, int originZ,
	BlockType type,
	bool isIlluminated)
{
	addFace(meshVertices, x, y, z, originX, originZ, type, 0, isIlluminated);
	addFace(meshVertices, x, y, z, originX, originZ, type, 1, isIlluminated);
	addFace(meshVertices, x, y, z, originX, originZ, type, 2, isIlluminated);
	addFace(meshVertices, x, y, z, originX, originZ, type, 3, isIlluminated);
	addFace(meshVertices, x, y, z, originX, originZ, type, 4, isIlluminated);
	addFace(meshVertices, x, y, z, originX, originZ, type, 5, isIlluminated);
}

void build2DInventoryCube(
	std::vector<float>& meshVertices,
	glm::vec2 origin,
	float scale,
	BlockType type)
{
	addInventoryFace(meshVertices, origin, scale, type, 2);
	addInventoryFace(meshVertices, origin, scale, type, 1);
	addInventoryFace(meshVertices, origin, scale, type, 0);
}

void addInventoryFace(
	std::vector<float>& meshVertices,
	glm::vec2 origin,				// inventory slot position
	float scale,
	BlockType type,
	int face)
{
    const float TILE_W = 1.0f / ATLAS_COLS;
    const float TILE_H = 1.0f / ATLAS_ROWS;

    glm::vec2 atlasOffset = getTextureOffset(type, face);
    const float flippedRow = static_cast<float>(ATLAS_ROWS - 1) - atlasOffset.y;
    glm::vec2 uvOffset = { atlasOffset.x * TILE_W, flippedRow * TILE_H };

    constexpr int quadToTri[6] = { 0,1,2, 2,3,0 };

    for (int i = 0; i < 6; ++i)
    {
        int v = quadToTri[i];

        glm::vec2 basePos = unitFacePositionsInventory[face][v];
        glm::vec2 pos = origin + basePos * scale;

        glm::vec2 uv = {
            uvTemplate[i].x * TILE_W + uvOffset.x + 10,
            uvTemplate[i].y * TILE_H + uvOffset.y
        };

        meshVertices.push_back(pos.x);
        meshVertices.push_back(pos.y);
        meshVertices.push_back(uv.x);
        meshVertices.push_back(uv.y);
    }
}

void addFace(
	std::vector<float>& meshVertices,
    float x, float y, float z,
    int originX, int originZ,
    BlockType type, int face,
    bool isIlluminated)
{
	const float TILE_W = 1.0f / ATLAS_COLS;
	const float TILE_H = 1.0f / ATLAS_ROWS;

    glm::vec2 atlasOffset = getTextureOffset(type, face);
    const float flippedRow = static_cast<float>(ATLAS_ROWS - 1) - atlasOffset.y;
    glm::vec2 uvOffset = { atlasOffset.x * TILE_W, flippedRow * TILE_H };

    glm::vec3 normal = faceNormals[face];

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
            uvTemplate[i].x * TILE_W + uvOffset.x,
            uvTemplate[i].y * TILE_H + uvOffset.y
        };

        // Pack vertex attributes
        meshVertices.push_back(pos.x);
        meshVertices.push_back(pos.y);
        meshVertices.push_back(pos.z);
        meshVertices.push_back(uv.x);
        meshVertices.push_back(uv.y);

        if (isIlluminated) {
            meshVertices.push_back(pos.y);     // gradient Y
            meshVertices.push_back(normal.x);
            meshVertices.push_back(normal.y);
            meshVertices.push_back(normal.z);
        }
    }
}
