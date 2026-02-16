
#ifndef BLOCK_RENDERING_HELPER_FUNCTIONS
#define BLOCK_RENDERING_HELPER_FUNCTIONS

#include <iostream>
#include <glm/glm.hpp>

#include "Item.hpp"

inline constexpr int ATLAS_COLS = 10;
inline constexpr int ATLAS_ROWS = 1;

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

glm::vec2 getTextureOffset(const BlockType type, const int face);

void buildCube(std::vector<float>& meshVertices, float x, float y, float z, int originX, int originZ, BlockType type, bool isIlluminated = false);
void build2DInventoryCube(std::vector<float>& meshVertices, glm::vec2 origin, float scale, BlockType type);
void addFace(std::vector<float>& meshVertices, float x, float y, float z, int originX, int originZ, BlockType type, int face, bool isIlluminated = false); // Add a face to the mesh vertices
void addInventoryFace(std::vector<float>& meshVertices, glm::vec2 origin, float scale, BlockType type, int face);

#endif