
#include "ChunkRenderer.hpp"

ChunkRenderer::ChunkRenderer(std::istream& in) : Chunk(in), meshVerticesSize(0), waterMeshVerticesSize(0) {}

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
        if (waterVAO) {
            glDeleteVertexArrays(1, &waterVAO);
            waterVAO = 0;
        }
        if (waterVBO) {
            glDeleteBuffers(1, &waterVBO);
            waterVBO = 0;
        }
    } else {
        VAO = 0;
        VBO = 0;
        waterVAO = 0;
        waterVBO = 0;
    }
}

void ChunkRenderer::updateMesh()
{
	buildMesh();
	needsUpdate = false;

	// //update possible neighbour
	if (neighbourNeedUpdate[WEST]) {
		if (auto westChunkBase = getAdjacentChunks()[WEST].lock()) {
			if (auto westChunk = std::dynamic_pointer_cast<ChunkRenderer>(westChunkBase)) {
				if (westChunk->hasAllAdjacentChunkLoaded())
					westChunk->buildMesh();
			}
		}
	}

	if (neighbourNeedUpdate[EAST]) {
		if (auto eastChunkBase = getAdjacentChunks()[EAST].lock()) {
			if (auto eastChunk = std::dynamic_pointer_cast<ChunkRenderer>(eastChunkBase)) {
				if (eastChunk->hasAllAdjacentChunkLoaded())
					eastChunk->buildMesh();
			}
		}
	}

	if (neighbourNeedUpdate[SOUTH]) {
		if (auto southChunkBase = getAdjacentChunks()[SOUTH].lock()) {
			if (auto southChunk = std::dynamic_pointer_cast<ChunkRenderer>(southChunkBase)) {
				if (southChunk->hasAllAdjacentChunkLoaded())
					southChunk->buildMesh();
			}
		}
	}

	if (neighbourNeedUpdate[NORTH]) {
		if (auto northChunkBase = getAdjacentChunks()[NORTH].lock()) {
			if (auto northChunk = std::dynamic_pointer_cast<ChunkRenderer>(northChunkBase)) {
				if (northChunk->hasAllAdjacentChunkLoaded())
					northChunk->buildMesh();
			}
		}
	}

	std::memset(neighbourNeedUpdate, 0, sizeof(neighbourNeedUpdate));
}

void ChunkRenderer::computeHeightMap(const std::vector<BlockType>& blockTypeVector) {
	// For each XZ column, find the highest opaque (solid) block.
	// heightMap stores Y+1 of the highest solid block, so any block at Y < heightMap[col]
	// is considered underground and won't receive directional sunlight.
	for (int x = 0; x < WIDTH; ++x) {
		for (int z = 0; z < DEPTH; ++z) {
			int topY = 0;
			for (int y = HEIGHT - 1; y >= 0; --y) {
				int idx = x + WIDTH * (y + HEIGHT * z);
				if (isBlockSolid(blockTypeVector[idx])) {
					topY = y + 1;
					break;
				}
			}
			heightMap[x * DEPTH + z] = topY;
		}
	}
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
    const float flippedRow = static_cast<float>(ATLAS_ROWS - 1) - tileCoord.y;
    glm::vec2 offset = { tileCoord.x * TILE_W, flippedRow * TILE_H };

    // Sky-light: 1.0 if this block face can see the sky, 0.0 if underground.
    // heightMap[col] = topY+1 of the highest solid block in the column.
    // A block at Y is the surface block when Y+1 == hm, so it should be lit.
    // Only blocks fully below the surface (y+1 < hm) are underground.
    int hm = heightMap[x * DEPTH + z];
    float skyLight = (y + 1 >= hm) ? 1.0f : 0.0f;

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
        meshVertices.push_back(skyLight); // sky exposure for shadow determination
    }
}

void ChunkRenderer::addWaterFace(int x, int y, int z, int face) {
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

    // Water sky-light
    int hm = heightMap[x * DEPTH + z];
    float skyLight = (y + 1 >= hm) ? 1.0f : 0.0f;

    // Build six vertices for this face
    for (int i = 0; i < 6; ++i) {
        float px = faceX + faceData[face][i * 3 + 0];
        float py = faceY + faceData[face][i * 3 + 1];
        float pz = faceZ + faceData[face][i * 3 + 2];

        float u = uvCoords[i * 2 + 0];
        float v = uvCoords[i * 2 + 1];

        waterMeshVertices.push_back(px);    // position.x
        waterMeshVertices.push_back(py);    // position.y
        waterMeshVertices.push_back(pz);    // position.z
        waterMeshVertices.push_back(u);     // texture u (unused by water shader)
        waterMeshVertices.push_back(v);     // texture v (unused by water shader)
        waterMeshVertices.push_back(py);    // Y for gradient
        waterMeshVertices.push_back(normal.x);
        waterMeshVertices.push_back(normal.y);
        waterMeshVertices.push_back(normal.z);
        waterMeshVertices.push_back(skyLight); // sky exposure
    }
}

void ChunkRenderer::buildMesh() {
	buildMeshData();
	uploadMesh();
}

void ChunkRenderer::buildMeshData() {
	meshVertices.clear();
	waterMeshVertices.clear();
	std::vector<BlockType> blockTypeVector;	// unpacked block indices

    // Decode palette indices to block types
    blockTypeVector.resize(WIDTH * HEIGHT * DEPTH);
    std::vector<uint32_t> decodedIndices;
    blockIndices.decodeAll(decodedIndices);

    for (size_t i = 0; i < blockTypeVector.size(); ++i) {
        uint32_t paletteIndex = decodedIndices[i];
        blockTypeVector[i] = palette[paletteIndex];
    }

    // Build per-column heightmap for sky-light determination
    computeHeightMap(blockTypeVector);

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
                BlockType currentBlock = blockTypeVector[idx];
                
                if (currentBlock == BlockType::AIR) continue;

                bool isWater = (currentBlock == BlockType::WATER);

                // FRONT (+Z)
                BlockType neighbor = getBlockOrNeighbor(x, y, z, 0, 0, +1, NORTH);
                if (isWater) {
                    // Water: only render face if neighbor is air
                    if (neighbor == BlockType::AIR) addWaterFace(x, y, z, 0);
                } else if (!isBlockSolid(neighbor)) {
                    // Solid block: render if neighbor is air or water
                    addFace(x, y, z, 0);
                }

                // BACK (-Z)
                neighbor = getBlockOrNeighbor(x, y, z, 0, 0, -1, SOUTH);
                if (isWater) {
                    if (neighbor == BlockType::AIR) addWaterFace(x, y, z, 1);
                } else if (!isBlockSolid(neighbor)) {
                    addFace(x, y, z, 1);
                }

                // TOP (+Y)
                neighbor = (y == HEIGHT - 1) ? BlockType::AIR : getBlockOrNeighbor(x, y, z, 0, +1, 0, NONE);
                if (isWater) {
                    if (neighbor == BlockType::AIR) addWaterFace(x, y, z, 2);
                } else if (!isBlockSolid(neighbor)) {
                    addFace(x, y, z, 2);
                }

                // BOTTOM (-Y)
                neighbor = (y == 0) ? BlockType::AIR : getBlockOrNeighbor(x, y, z, 0, -1, 0, NONE);
                if (isWater) {
                    if (neighbor == BlockType::AIR) addWaterFace(x, y, z, 3);
                } else if (!isBlockSolid(neighbor)) {
                    addFace(x, y, z, 3);
                }

                // RIGHT (+X)
                neighbor = getBlockOrNeighbor(x, y, z, +1, 0, 0, EAST);
                if (isWater) {
                    if (neighbor == BlockType::AIR) addWaterFace(x, y, z, 4);
                } else if (!isBlockSolid(neighbor)) {
                    addFace(x, y, z, 4);
                }

                // LEFT (-X)
                neighbor = getBlockOrNeighbor(x, y, z, -1, 0, 0, WEST);
                if (isWater) {
                    if (neighbor == BlockType::AIR) addWaterFace(x, y, z, 5);
                } else if (!isBlockSolid(neighbor)) {
                    addFace(x, y, z, 5);
                }
            }
        }
    }
}

void ChunkRenderer::uploadMesh() {
    // Upload solid mesh to OpenGL
    if (VAO == 0)
        glGenVertexArrays(1, &VAO);
    if (VBO == 0)
        glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(float), meshVertices.data(), GL_STATIC_DRAW);

    GLsizei stride = 10 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, static_cast<void *>(nullptr));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(9 * sizeof(float)));
    glEnableVertexAttribArray(4);
    
    meshVerticesSize = meshVertices.size();
    meshVertices.clear();
    meshVertices.shrink_to_fit();

    // Upload water mesh to OpenGL
    if (waterMeshVertices.size() > 0) {
        if (waterVAO == 0)
            glGenVertexArrays(1, &waterVAO);
        if (waterVBO == 0)
            glGenBuffers(1, &waterVBO);

        glBindVertexArray(waterVAO);
        glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
        glBufferData(GL_ARRAY_BUFFER, waterMeshVertices.size() * sizeof(float), waterMeshVertices.data(), GL_STATIC_DRAW);

        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, static_cast<void *>(nullptr));
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(3 * sizeof(float)));
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(5 * sizeof(float)));
        glEnableVertexAttribArray(2);
        glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(6 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(9 * sizeof(float)));
        glEnableVertexAttribArray(4);
        
        waterMeshVerticesSize = waterMeshVertices.size();
    } else {
        waterMeshVerticesSize = 0;
    }
    
    waterMeshVertices.clear();
    waterMeshVertices.shrink_to_fit();
}
