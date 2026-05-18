
#ifndef BLOCK_RENDERING_HELPER_FUNCTIONS
#define BLOCK_RENDERING_HELPER_FUNCTIONS

#include <iostream>
#include <glm/glm.hpp>

#include "Item.hpp"

class TextureManager; // Forward declaration

inline constexpr glm::vec3 unitFacePositions[6][6] = {
    // FRONT (+Z)
    { {0,0,1}, {1,0,1}, {1,1,1},
      {1,1,1}, {0,1,1}, {0,0,1} },

    // BACK (-Z)
    { {1,0,0}, {0,0,0}, {0,1,0},
      {0,1,0}, {1,1,0}, {1,0,0} },

    // TOP (+Y)
    { {0,1,1}, {1,1,1}, {1,1,0},
      {1,1,0}, {0,1,0}, {0,1,1} },

    // BOTTOM (-Y)
    { {0,0,0}, {1,0,0}, {1,0,1},
      {1,0,1}, {0,0,1}, {0,0,0} },

    // RIGHT (+X)
    { {1,0,1}, {1,0,0}, {1,1,0},
      {1,1,0}, {1,1,1}, {1,0,1} },

    // LEFT (-X)
    { {0,0,0}, {0,0,1}, {0,1,1},
      {0,1,1}, {0,1,0}, {0,0,0} }
};

inline constexpr glm::vec2 uvTemplate[6] = {
    {0,0}, {1,0}, {1,1},
    {1,1}, {0,1}, {0,0}
};

inline constexpr glm::vec3 faceNormals[6] = {
    {0,0,1}, {0,0,-1}, {0,1,0},
    {0,-1,0}, {1,0,0}, {-1,0,0}
};

inline constexpr glm::vec2 unitFacePositionsInventory[3][4] = 
{
	{ {0, 0.25}, {0.5, 0}, {0.5,0.5}, {0, 0.75}},		//BOTTOM  LEFT
	{ {0.5, 0}, {1, 0.25}, {1, 0.75}, {0.5,0.5}},		//BOTTOM RIGHT
	{ {0.5,0.5}, {1, 0.75}, {0.5,1}, {0, 0.75} },		//TOP
};

// `skyFactor` is appended as a 7th per-vertex float when isIlluminated=false
// (dropped-item path). It's 0..1 sky-light at the item's world position so the
// fragment shader can darken caves the same way terrain does. Ignored when
// isIlluminated=true (chunk meshing uses ChunkRenderer::addFace, not this one).
void buildCube(std::vector<float>& meshVertices, float x, float y, float z, int originX, int originZ, BlockType type, const TextureManager* texMgr = nullptr, bool isIlluminated = false, float skyFactor = 1.0f, float blockFactor = 0.0f);
void build2DInventoryCube(std::vector<float>& meshVertices, glm::vec2 origin, float scale, BlockType type, const TextureManager* texMgr = nullptr);
void addFace(std::vector<float>& meshVertices, float x, float y, float z, int originX, int originZ, BlockType type, int face, const TextureManager* texMgr = nullptr, bool isIlluminated = false, float skyFactor = 1.0f, float blockFactor = 0.0f); // Add a face to the mesh vertices
void addInventoryFace(std::vector<float>& meshVertices, glm::vec2 origin, float scale, BlockType type, int face, const TextureManager* texMgr = nullptr);

// Dropped vegetation rendered as a 2D-looking sprite (cross of two
// perpendicular quads, like world vegetation). Vertices are emitted in
// camera-relative space and the buffer is padded to 36 verts to match
// ItemPropEntityManager::ITEM_SIZE. Format matches buildCube:
// pos(3) + uv(2) + texLayer(1) + skyLight(1) per vertex.
void buildItemSprite(std::vector<float>& meshVertices, float x, float y, float z, int texLayer, float skyFactor = 1.0f, float blockFactor = 0.0f);

// Dropped tool / ingot / misc item rendered as a Y-axis-billboarded quad
// (single flat sprite that always faces the camera horizontally). Same
// vertex format and padding rules as buildItemSprite. itemRel is the item
// position in camera-relative space (item - eye, double-precision cancel),
// which doubles as the "to-camera" direction (negated and projected on XZ).
void buildItemBillboard(std::vector<float>& meshVertices, const glm::dvec3& itemRel, int texLayer, float skyFactor = 1.0f, float blockFactor = 0.0f);

// Inventory icon for items (vegetation, weapons, misc): a single flat quad,
// no isometric perspective. Same vertex format as addInventoryFace:
// pos(2) + uv(2) + texLayer(1) per vertex, 6 verts (two triangles).
void build2DInventorySprite(std::vector<float>& meshVertices, glm::vec2 origin, float scale, int texLayer);

#endif