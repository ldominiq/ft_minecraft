
#ifndef BLOCK_RENDERING_HELPER_FUNCTIONS
#define BLOCK_RENDERING_HELPER_FUNCTIONS

#include <iostream>
#include <glm/glm.hpp>

#include "Item.hpp"

inline constexpr int ATLAS_COLS = 10;
inline constexpr int ATLAS_ROWS = 2;

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

glm::vec2 getTextureOffset(const BlockType type, const int face);

void buildCube(std::vector<float>& meshVertices, float x, float y, float z, int originX, int originZ, BlockType type, bool isIlluminated = false);
void addFace(std::vector<float>& meshVertices, float x, float y, float z, int originX, int originZ, BlockType type, int face, bool isIlluminated = false); // Add a face to the mesh vertices


#endif