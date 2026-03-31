
#include "ChunkRenderer.hpp"

ChunkRenderer::ChunkRenderer(std::istream& in) : Chunk(in), meshVerticesSize(0), waterMeshVerticesSize(0) {
    vegetationRenderer = std::make_unique<VegetationRenderer>();
}

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
		if (const auto westChunkBase = getAdjacentChunks()[WEST].lock()) {
			if (const auto westChunk = std::dynamic_pointer_cast<ChunkRenderer>(westChunkBase)) {
				if (westChunk->hasAllAdjacentChunkLoaded())
					westChunk->buildMesh();
			}
		}
	}

	if (neighbourNeedUpdate[EAST]) {
		if (const auto eastChunkBase = getAdjacentChunks()[EAST].lock()) {
			if (const auto eastChunk = std::dynamic_pointer_cast<ChunkRenderer>(eastChunkBase)) {
				if (eastChunk->hasAllAdjacentChunkLoaded())
					eastChunk->buildMesh();
			}
		}
	}

	if (neighbourNeedUpdate[SOUTH]) {
		if (const auto southChunkBase = getAdjacentChunks()[SOUTH].lock()) {
			if (const auto southChunk = std::dynamic_pointer_cast<ChunkRenderer>(southChunkBase)) {
				if (southChunk->hasAllAdjacentChunkLoaded())
					southChunk->buildMesh();
			}
		}
	}

	if (neighbourNeedUpdate[NORTH]) {
		if (const auto northChunkBase = getAdjacentChunks()[NORTH].lock()) {
			if (const auto northChunk = std::dynamic_pointer_cast<ChunkRenderer>(northChunkBase)) {
				if (northChunk->hasAllAdjacentChunkLoaded())
					northChunk->buildMesh();
			}
		}
	}

	std::memset(neighbourNeedUpdate, 0, sizeof(neighbourNeedUpdate));
}

void ChunkRenderer::addFace(const int x, const int y, const int z, const BlockType type, const int face, const float skyLightLevel) {
    const float faceX = static_cast<float>(originX + x);
    const float faceY = static_cast<float>(y);
    const float faceZ = static_cast<float>(originZ + z);

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

    // Get texture layer for this block face from TextureManager
    float texLayer = 0.0f;
    if (textureManager) {
        const BlockTextures& bt = textureManager->getBlockTextures(type);
        texLayer = static_cast<float>(bt.getLayerForFace(face));
        if (type == BlockType::GRASS && face == 2) {
            texLayer = textureManager->getGrassTintLayer(getBiomeAt(x, z));
        }
    }

    // Build six vertices for this face using the computed light
    bool isCactusSide = (type == BlockType::CACTUS && face != 2 && face != 3);
    constexpr float cactusInset = 1.0f / 16.0f;

    for (int i = 0; i < 6; ++i) {
        float px = faceX + faceData[face][i * 3 + 0];
        float py = faceY + faceData[face][i * 3 + 1];
        float pz = faceZ + faceData[face][i * 3 + 2];

        // Cactus: inset side faces by 1/16 of a block
        if (isCactusSide) {
            if (face == 0) pz = faceZ + 1.0f - cactusInset;   // front (Z+): pull inward
            if (face == 1) pz = faceZ + cactusInset;           // back  (Z-): push inward
            if (face == 4) px = faceX + 1.0f - cactusInset;   // right (X+): pull inward
            if (face == 5) px = faceX + cactusInset;           // left  (X-): push inward
        }

        float baseU = uvCoords[i * 2 + 0]; // 0 → 1
        float baseV = uvCoords[i * 2 + 1]; // 0 → 1

        float u = baseU;
        float v = baseV;

        meshVertices.push_back(px);        // position.x
        meshVertices.push_back(py);        // position.y
        meshVertices.push_back(pz);        // position.z
        meshVertices.push_back(u);         // texture u
        meshVertices.push_back(v);         // texture v
        meshVertices.push_back(texLayer);  // texture array layer
        meshVertices.push_back(py);        // send Y again for gradient
        meshVertices.push_back(normal.x);
        meshVertices.push_back(normal.y);
        meshVertices.push_back(normal.z);
        meshVertices.push_back(skyLightLevel); // sky-light (0.0 = dark, 1.0 = full sun)
    }
}

void ChunkRenderer::addWaterFace(const int x, const int y, const int z, const int face, const float skyLightLevel) {
    const float faceX = static_cast<float>(originX + x);
    const float faceY = static_cast<float>(y);
    const float faceZ = static_cast<float>(originZ + z);

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
        waterMeshVertices.push_back(0.0f);  // texture layer (unused by water shader)
        waterMeshVertices.push_back(py);    // Y for gradient
        waterMeshVertices.push_back(normal.x);
        waterMeshVertices.push_back(normal.y);
        waterMeshVertices.push_back(normal.z);
        waterMeshVertices.push_back(skyLightLevel); // sky-light (0.0 = dark, 1.0 = full sun)
    }
}

void ChunkRenderer::buildMesh() {
	// When called from updateMesh() (block placed/broken), we need to
	// recompute sky-light because the terrain changed.  This runs
	// single-threaded here so there's no race with neighbors.
	computeSkyLight();
	buildMeshData();
	uploadMesh();
	buildVegetationMesh();
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

    // Look up the sky-light value for a face.
    //
    // We want the light level of the AIR block that the face is
    // exposed to — that tells us how much sky exposure this face has.
    //
    // For faces within this chunk: straightforward array lookup.
    //
    // For faces at chunk borders: we read the adjacent chunk's
    // skyLight array.  This is safe because computeSkyLight() uses
    // std::swap — the member array is either the previous fully-
    // computed result or the new one, never a partial write.
    // If the neighbor hasn't computed skyLight yet (empty array),
    // getSkyLight() returns 15 (assume sunlit — corrected on rebuild).
    auto getSkyLightForFace = [&](int blockX, int blockY, int blockZ,
                                   int dx, int dy, int dz,
                                   Direction dir) -> uint8_t {
        int neighborX = blockX + dx;
        int neighborY = blockY + dy;
        int neighborZ = blockZ + dz;

        // Above world top = full sunlight
        if (neighborY >= HEIGHT) return 15;
        // Below world bottom = full darkness
        if (neighborY < 0) return 0;

        // Neighbor is within this chunk → direct lookup
        if (neighborX >= 0 && neighborX < WIDTH &&
            neighborZ >= 0 && neighborZ < DEPTH) {
            return getSkyLight(neighborX, neighborY, neighborZ);
        }

        // Neighbor is in an adjacent chunk → read its skyLight
        const auto adjacentChunk = adjacentChunks[dir].lock();
        if (!adjacentChunk) return 15; // Not loaded yet, assume sunlit

        const int remappedX = (dx == -1 ? WIDTH - 1 : (dx == 1 ? 0 : blockX));
        const int remappedZ = (dz == -1 ? DEPTH - 1 : (dz == 1 ? 0 : blockZ));
        return adjacentChunk->getSkyLight(remappedX, neighborY, remappedZ);
    };

    struct FaceDir {
        int dx, dy, dz;
        Direction neighborDir;
        int faceIndex;
    };

    static constexpr FaceDir faces[6] = {
        {  0,  0, +1, NORTH, 0 }, // front
        {  0,  0, -1, SOUTH, 1 }, // back
        {  0, +1,  0, NONE,  2 }, // top
        {  0, -1,  0, NONE,  3 }, // bottom
        { +1,  0,  0, EAST,  4 }, // right
        { -1,  0,  0, WEST,  5 }  // left
    };

    // Helper: convert a sky-light value (0–15) to a 0.0–1.0 float
    // for the vertex data.  We do this once per face.
    auto lightToFloat = [](const uint8_t lightVal) -> float {
        return static_cast<float>(lightVal) / 15.0f;
    };

    for (int x = 0; x < WIDTH; ++x) {
        for (int y = 0; y < HEIGHT; ++y) {
            for (int z = 0; z < DEPTH; ++z) {
                const int idx = x + WIDTH * (y + HEIGHT * z);
                BlockType currentBlock = blockTypeVector[idx];
                
                if (currentBlock == BlockType::AIR) continue;
                if (isBlockVegetation(currentBlock)) continue;

                const bool isWater = (currentBlock == BlockType::WATER);

                for (const FaceDir& face : faces) {
                    BlockType neighborBlock;
                    if ((face.dy == +1 && y == HEIGHT - 1) || (face.dy == -1 && y == 0)) {
                        neighborBlock = BlockType::AIR; // world edge
                    } else {
                        neighborBlock = getBlockOrNeighbor(x, y, z, face.dx, face.dy, face.dz, face.neighborDir);
                    }

                    const float faceSkyLight = lightToFloat(getSkyLightForFace(x, y, z, face.dx, face.dy, face.dz, face.neighborDir));

                    if (isWater) {
                        if (neighborBlock == BlockType::AIR || isBlockTransparent(neighborBlock) || isBlockVegetation(neighborBlock)) {
                            addWaterFace(x, y, z, face.faceIndex, faceSkyLight);
                        }
                    } else if (currentBlock == BlockType::CACTUS) {
                        bool isSide = (face.faceIndex != 2 && face.faceIndex != 3);
                        if (isSide || !isBlockSolid(neighborBlock) || neighborBlock != BlockType::CACTUS) {
                            addFace(x, y, z, currentBlock, face.faceIndex, faceSkyLight);
                        }
                    } else if (!isBlockSolid(neighborBlock) || isBlockTransparent(neighborBlock) || neighborBlock == BlockType::CACTUS) {
                        addFace(x, y, z, currentBlock, face.faceIndex, faceSkyLight);
                    }
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

    // Vertex layout (11 floats per vertex):
    //   location 0: position  (vec3)  — floats 0-2
    //   location 1: texCoord  (vec2)  — floats 3-4
    //   location 2: texLayer  (float) — float  5
    //   location 3: gradientY (float) — float  6
    //   location 4: normal    (vec3)  — floats 7-9
    //   location 5: skyLight  (float) — float  10
    GLsizei stride = 11 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, static_cast<void *>(nullptr));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(5 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(6 * sizeof(float)));
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(7 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(10 * sizeof(float)));
    glEnableVertexAttribArray(5);
    
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
        glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(6 * sizeof(float)));
        glEnableVertexAttribArray(3);
        glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(7 * sizeof(float)));
        glEnableVertexAttribArray(4);
        glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(10 * sizeof(float)));
        glEnableVertexAttribArray(5);
        
        waterMeshVerticesSize = waterMeshVertices.size();
    } else {
        waterMeshVerticesSize = 0;
    }
    
    waterMeshVertices.clear();
    waterMeshVertices.shrink_to_fit();
}

void ChunkRenderer::buildVegetationMesh() const {
    if (!vegetationRenderer || !textureManager)
        return;

    vegetationRenderer->setTextureManager(textureManager);

    // Sea vegetation comes from the vegetation list (not stored in the block grid).
    // Land vegetation is derived by scanning the block grid directly.
    std::vector<Chunk::VegetationInstance> vegInstances;
    vegInstances.reserve(vegetation.size());

    for (const auto& v : vegetation) {
        vegInstances.push_back(v); // sea veg only
    }

    for (int lx = 0; lx < WIDTH; ++lx) {
        for (int lz = 0; lz < DEPTH; ++lz) {
            for (int ly = 0; ly < HEIGHT; ++ly) {
                const BlockType b = getBlock(lx, ly, lz);
                if (isBlockVegetation(b)) {
                    vegInstances.push_back({
                        static_cast<uint8_t>(lx),
                        static_cast<uint8_t>(ly),
                        static_cast<uint8_t>(lz),
                        b
                    });
                }
            }
        }
    }

    if (!vegInstances.empty()) {
        vegetationRenderer->buildInstances(vegInstances.data(), vegInstances.size(), originX, originZ, this);
        vegetationRenderer->uploadMesh();
    } else {
        vegetationRenderer->clearInstances();
    }
}
