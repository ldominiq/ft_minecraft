
#include "ChunkRenderer.hpp"

ChunkRenderer::ChunkRenderer(std::istream& in) : Chunk(in) {}

ChunkRenderer::~ChunkRenderer() {
    if (glfwGetCurrentContext()) {
        if (VAO) {
            glDeleteVertexArrays(1, &VAO);
            VAO = 0;
        }
        if (VBO) {
            glDeleteBuffers(1, &VBO);
            VBO = 0;
        }
    } else {
        VAO = 0;
        VBO = 0;
    }
}

// This function maps block type + face to UV offset
glm::vec2 ChunkRenderer::getTextureOffset(const BlockType type, const int face) {
    int col = 0;
    int row = 0;

    switch (type) {
        case BlockType::GRASS:
            if (face == 2)      { col = 0; row = 0; } // top
            else if (face == 3) { col = 2; row = 0; } // bottom = dirt
            else                { col = 1; row = 0; } // side = grass-side
            break;

        case BlockType::DIRT:
            col = 2; row = 0;
            break;

        case BlockType::STONE:
            col = 3; row = 0;
            break;

        case BlockType::SAND:
            col = 4; row = 0;
            break;

        case BlockType::SNOW:
            col = 5; row = 0;
            break;

        case BlockType::WATER:
            col = 6; row = 0;
            break;
            
        case BlockType::BEDROCK:
            col = 7; row = 0;
            break;

        case BlockType::LOG:
            col = 8; row = 0;
            break;
        case BlockType::LEAVES:
            col = 9; row = 0;
            break;

        default:
            col = 0; row = 0;
            std::cerr << "Unknown BlockType in getTextureOffset: " << static_cast<int>(type) << std::endl;
            break;
    }

    return glm::vec2(col, row);
}

void ChunkRenderer::addFace(int x, int y, int z, int face) {
    const float faceX = static_cast<float>(originX + x);
    const float faceY = static_cast<float>(y);
    const float faceZ = static_cast<float>(originZ + z);

    const float TILE_W = 1.0f / ATLAS_COLS;
    const float TILE_H = 1.0f / ATLAS_ROWS;

    static const float faceData[6][18] = {
        // FRONT face (Z+)
        { 0,0,1,  1,0,1,  1,1,1,
        1,1,1,  0,1,1,  0,0,1 },

        // BACK face (Z-)
        { 1,0,0,  0,0,0,  0,1,0,
        0,1,0,  1,1,0,  1,0,0 },

        // TOP face (Y+)
        { 0,1,1,  1,1,1,  1,1,0,
        1,1,0,  0,1,0,  0,1,1 },

        // BOTTOM face (Y-)
        { 0,0,0,  1,0,0,  1,0,1,
        1,0,1,  0,0,1,  0,0,0 },

        // RIGHT face (X+)
        { 1,0,1,  1,0,0,  1,1,0,
        1,1,0,  1,1,1,  1,0,1 },

        // LEFT face (X-)
        { 0,0,0,  0,0,1,  0,1,1,
        0,1,1,  0,1,0,  0,0,0 }
    };

    static const float uvCoords[12] = {
        0, 0,
        1, 0,
        1, 1,
        1, 1,
        0, 1,
        0, 0
    };

    static const glm::vec3 faceNormals[6] = {
        {  0,  0,  1 }, // front
        {  0,  0, -1 }, // back
        {  0,  1,  0 }, // top
        {  0, -1,  0 }, // bottom
        {  1,  0,  0 }, // right
        { -1,  0,  0 }  // left
    };

    glm::vec3 normal = faceNormals[face];

    // Get block type for this position
    const BlockType type = getBlock(x, y, z);

    // Determine UV offset in atlas based on block type and face
    glm::vec2 tileCoord = getTextureOffset(type, face);
    glm::vec2 offset = { tileCoord.x * TILE_W, tileCoord.y * TILE_H };

    // Build six vertices for this face using the computed light
    for (int i = 0; i < 6; ++i) {
        float px = faceX + faceData[face][i * 3 + 0];
        float py = faceY + faceData[face][i * 3 + 1];
        float pz = faceZ + faceData[face][i * 3 + 2];

        float baseU = uvCoords[i * 2 + 0]; // 0 → 1
        float baseV = uvCoords[i * 2 + 1]; // 0 → 1

        float u = baseU * TILE_W + offset.x;
        float v = baseV * TILE_H + offset.y;

        meshVertices.push_back(px);    // position.x
        meshVertices.push_back(py);    // position.y
        meshVertices.push_back(pz);    // position.z
        meshVertices.push_back(u);     // texture u
        meshVertices.push_back(v);     // texture v
        meshVertices.push_back(py);    // send Y again for gradient
        meshVertices.push_back(normal.x);
        meshVertices.push_back(normal.y);
        meshVertices.push_back(normal.z);
    }
}

void ChunkRenderer::setAdjacentChunks(const int direction, std::shared_ptr<ChunkRenderer> &chunk){
    adjacentChunks[direction] = chunk;
}

bool ChunkRenderer::hasAllAdjacentChunkLoaded() const {
    for (const auto& adj : adjacentChunks) {
        if (adj.expired()) {
            return false;
        }
    }
    return true;
}

void ChunkRenderer::buildMesh() {
	buildMeshData();
	uploadMesh();
}

void ChunkRenderer::buildMeshData() {
	meshVertices.clear();
	std::vector<BlockType> blockTypeVector;	// unpacked block indices

    // Decode palette indices to block types
    blockTypeVector.resize(WIDTH * HEIGHT * DEPTH);
    std::vector<uint32_t> decodedIndices;
    blockIndices.decodeAll(decodedIndices);

    for (size_t i = 0; i < blockTypeVector.size(); ++i) {
        uint32_t paletteIndex = decodedIndices[i];
        blockTypeVector[i] = palette[paletteIndex];
    }

    auto getBlockOrNeighbor = [&](int x, int y, int z, int dx, int dy, int dz, Direction dir) -> BlockType {
        if (x + dx < 0 || x + dx >= WIDTH ||
            z + dz < 0 || z + dz >= DEPTH) 
        {
            auto neighbor = adjacentChunks[dir].lock();
            if (!neighbor) return BlockType::AIR;
            int nx = (dx == -1 ? WIDTH - 1 : (dx == 1 ? 0 : x));
            int nz = (dz == -1 ? DEPTH - 1 : (dz == 1 ? 0 : z));
            return neighbor->getBlock(nx, y + dy, nz);
        }
        if (y + dy < 0 || y + dy >= HEIGHT) 
            return BlockType::AIR; // top or bottom world edge inside chunk
        return blockTypeVector[(x + dx) + WIDTH * ((y + dy) + HEIGHT * (z + dz))];
    };

    for (int x = 0; x < WIDTH; ++x) {
        for (int y = 0; y < HEIGHT; ++y) {
            for (int z = 0; z < DEPTH; ++z) {
                int idx = x + WIDTH * (y + HEIGHT * z);
                if (blockTypeVector[idx] == BlockType::AIR) continue;

                // FRONT (+Z)
                if (getBlockOrNeighbor(x, y, z, 0, 0, +1, NORTH) == BlockType::AIR)
                    addFace(x, y, z, 0);

                // BACK (-Z)
                if (getBlockOrNeighbor(x, y, z, 0, 0, -1, SOUTH) == BlockType::AIR)
                    addFace(x, y, z, 1);

                // TOP (+Y) – no vertical neighbor chunks
                if (y == HEIGHT - 1 || getBlockOrNeighbor(x, y, z, 0, +1, 0, NONE) == BlockType::AIR)
                    addFace(x, y, z, 2);

                // BOTTOM (-Y)
                if (y == 0 || getBlockOrNeighbor(x, y, z, 0, -1, 0, NONE) == BlockType::AIR)
                    addFace(x, y, z, 3);

                // RIGHT (+X)
                if (getBlockOrNeighbor(x, y, z, +1, 0, 0, EAST) == BlockType::AIR)
                    addFace(x, y, z, 4);

                // LEFT (-X)
                if (getBlockOrNeighbor(x, y, z, -1, 0, 0, WEST) == BlockType::AIR)
                    addFace(x, y, z, 5);
            }
        }
    }
}

void ChunkRenderer::uploadMesh() {
    // Upload mesh to OpenGL (unchanged)
    if (VAO == 0)
        glGenVertexArrays(1, &VAO);
    if (VBO == 0)
        glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(float), meshVertices.data(), GL_STATIC_DRAW);

    GLsizei stride = 9 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, static_cast<void *>(nullptr));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    
    meshVerticesSize = meshVertices.size();
    meshVertices.clear();
    meshVertices.shrink_to_fit();
}
