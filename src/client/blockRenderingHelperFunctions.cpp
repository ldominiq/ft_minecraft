
#include "blockRenderingHelperFunctions.hpp"

glm::vec2 getTextureOffset(BlockType type, int face) {
	static int grassOffset = 1; //grass takes 2 blocks.. same with logs potentially :/
    int id = -grassOffset + static_cast<int>(type) - static_cast<int>(BlockType::BEGIN);
    int col = id % ATLAS_COLS;
    int row = id / ATLAS_COLS;

    switch (type) {
        case BlockType::GRASS:
            if (face == 2)      { col = 0; row = 0; } // top
            else if (face == 3) { col = 2; row = 0; } // bottom = dirt
            else                { col = 1; row = 0; } // side = grass-side
            break;

        case BlockType::LOG:
            if (face == 2 || face == 3) { col = 8; row = 1; } // top/bottom = rings
            else                        { col = 8; row = 0; }  // sides = bark
            break;

        default:
            break; // use auto-calculated col/row
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
    glm::vec2 uvOffset = { atlasOffset.x * TILE_W, atlasOffset.y * TILE_H };

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

        glm::vec2 uv = { uvTemplate[i].x * TILE_W + uvOffset.x,
                         uvTemplate[i].y * TILE_H + uvOffset.y };

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
