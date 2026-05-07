
#include "ChunkRenderer.hpp"

// Default to "Smart" — keep alpha cutouts on outer leaf surfaces, but cull
// the wasted internal faces. Closest match to the original look at a fraction
// of the vertex count.
ChunkRenderer::LeafRenderMode ChunkRenderer::sLeafRenderMode = ChunkRenderer::LeafRenderMode::Smart;

ChunkRenderer::ChunkRenderer(std::istream& in) : Chunk(in), meshVertexCount(0), waterMeshVertexCount(0) {
    vegetationRenderer = std::make_unique<VegetationRenderer>();
    cachedMinP = glm::vec3(static_cast<float>(originX), 0.0f, static_cast<float>(originZ));
    cachedMaxP = glm::vec3(static_cast<float>(originX) + Chunk::WIDTH, static_cast<float>(Chunk::HEIGHT), static_cast<float>(originZ) + Chunk::DEPTH);
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
    // Mesh vertices are baked in chunk-LOCAL coordinates so that the GPU
    // never sees the large world coordinate of the chunk origin. The
    // origin is applied per-draw via the chunkOriginWorld / chunkRel
    // uniforms. This is what keeps geometry rock-stable far from origin.
    const float faceX = static_cast<float>(x);
    const float faceY = static_cast<float>(y);
    const float faceZ = static_cast<float>(z);

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

    // Get texture layer for this block face from TextureManager
    uint32_t texLayer = 0;
    if (textureManager) {
        const BlockTextures& bt = textureManager->getBlockTextures(type);
        texLayer = static_cast<uint32_t>(bt.getLayerForFace(face));
        if (type == BlockType::GRASS && face == 2) {
            texLayer = static_cast<uint32_t>(textureManager->getGrassTintLayer(getBiomeAt(x, z)));
        }
    }

    // Build six vertices for this face using the computed light
    bool isCactusSide = (type == BlockType::CACTUS && face != 2 && face != 3);
    constexpr float cactusInset = 1.0f / 16.0f;

    // Quad corner index per quad-vertex. The 6 verts of a face form two
    // triangles {0,1,2} {0,2,3} so corners go 0,1,2, 2,3,0. UVs are
    // reconstructed in the vertex shader from CORNERS[cornerIdx].
    const uint32_t normalIdx = static_cast<uint32_t>(face); // matches NORMALS[] in shader

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

        meshVertices.push_back(packed_vertex::pack(
            px, py, pz,
            normalIdx,
            packed_vertex::CORNER_FOR_VERT[i],
            texLayer,
            skyLightLevel));
    }
}

void ChunkRenderer::addWaterFace(const int x, const int y, const int z, const int face, const float skyLightLevel) {
    const float faceX = static_cast<float>(x);
    const float faceY = static_cast<float>(y);
    const float faceZ = static_cast<float>(z);

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

    // Water shader currently only reads position, but we still encode
    // normal/corner/skyLight so the format stays uniform with terrain.
    const uint32_t normalIdx = static_cast<uint32_t>(face);

    for (int i = 0; i < 6; ++i) {
        float px = faceX + faceData[face][i * 3 + 0];
        float py = faceY + faceData[face][i * 3 + 1];
        float pz = faceZ + faceData[face][i * 3 + 2];

        waterMeshVertices.push_back(packed_vertex::pack(
            px, py, pz,
            normalIdx,
            packed_vertex::CORNER_FOR_VERT[i],
            /*texLayer=*/0u,
            skyLightLevel));
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

    // In Fast/Smart, leaves are treated as opaque blocks for mesh-emission
    // decisions: faces between leaves and other leaves (or between leaves
    // and other solid blocks) are skipped. In Fancy, leaves stay transparent
    // and every face is emitted — the original behaviour.
    const bool leavesAreTransparentForMesh =
        (sLeafRenderMode == LeafRenderMode::Fancy);

    for (int x = 0; x < WIDTH; ++x) {
        for (int y = 0; y < HEIGHT; ++y) {
            for (int z = 0; z < DEPTH; ++z) {
                const int idx = x + WIDTH * (y + HEIGHT * z);
                BlockType currentBlock = blockTypeVector[idx];

                if (currentBlock == BlockType::AIR) continue;
                if (isBlockVegetation(currentBlock)) continue;

                const bool isWater = (currentBlock == BlockType::WATER);
                const bool currentIsLeaf = isBlockLeaves(currentBlock);
                const bool smartLeavesCullLikeFast = (sLeafRenderMode == LeafRenderMode::Smart);
                const bool fancyLeaves = (sLeafRenderMode == LeafRenderMode::Fancy);

                for (const FaceDir& face : faces) {
                    BlockType neighborBlock;
                    if ((face.dy == +1 && y == HEIGHT - 1) || (face.dy == -1 && y == 0)) {
                        neighborBlock = BlockType::AIR; // world edge
                    } else {
                        neighborBlock = getBlockOrNeighbor(x, y, z, face.dx, face.dy, face.dz, face.neighborDir);
                    }

                    const float faceSkyLight = lightToFloat(getSkyLightForFace(x, y, z, face.dx, face.dy, face.dz, face.neighborDir));
                    const bool neighborIsLeaf = isBlockLeaves(neighborBlock);
                    const bool neighborIsTransparent = isBlockTransparent(neighborBlock);

                    // Treat leaves as transparent (Fancy) or opaque (Fast/Smart)
                    // for the purposes of the emission test below. Cactus is
                    // always transparent; this only affects leaf neighbours.
                    const bool neighborTreatedTransparent =
                        isBlockTransparent(neighborBlock) &&
                        (leavesAreTransparentForMesh || !isBlockLeaves(neighborBlock));

                    if (isWater) {
                        if (neighborBlock == BlockType::AIR || isBlockTransparent(neighborBlock) || isBlockVegetation(neighborBlock)) {
                            addWaterFace(x, y, z, face.faceIndex, faceSkyLight);
                        }
                    } else if (currentBlock == BlockType::CACTUS) {
                        bool isSide = (face.faceIndex != 2 && face.faceIndex != 3);
                        if (isSide || !isBlockSolid(neighborBlock) || neighborBlock != BlockType::CACTUS) {
                            addFace(x, y, z, currentBlock, face.faceIndex, faceSkyLight);
                        }
                    }
                    else if (currentIsLeaf) {
                        // Leaves:
                        // - Fast/Smart: cull leaf-to-leaf faces
                        // - Fancy: keep leaf-to-leaf faces
                        if (!isBlockSolid(neighborBlock) ||
                            neighborTreatedTransparent ||
                            (fancyLeaves && neighborIsLeaf) ||
                            neighborBlock == BlockType::CACTUS) {
                            addFace(x, y, z, currentBlock, face.faceIndex, faceSkyLight);
                        }
                    }
                    else {
                        // Solid blocks:
                        // - Smart/Fancy: render faces next to leaves
                        // - Fast: cull them
                        const bool neighborTreatsAsTransparent =
                            neighborIsTransparent &&
                            (fancyLeaves || !neighborIsLeaf);

                        if (!isBlockSolid(neighborBlock) ||
                            neighborTreatsAsTransparent ||
                            (smartLeavesCullLikeFast && neighborIsLeaf) ||
                            neighborBlock == BlockType::CACTUS) {
                            addFace(x, y, z, currentBlock, face.faceIndex, faceSkyLight);
                        }
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
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(PackedVertex), meshVertices.data(), GL_STATIC_DRAW);

    // Packed terrain vertex layout (8 bytes per vertex). Decoded in
    // shaders/terrain_vertex_decode.glsl.
    //   location 0: v0  (uint) — pos.x | pos.y | pos.z (1/16 fixed point)
    //   location 1: v1  (uint) — normal | corner | texLayer | skyLight
    // NOTE: glVertexAttribIPointer (the I variant) — integer attributes are
    // delivered as uint without the float conversion path.
    GLsizei stride = sizeof(PackedVertex);
    glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, stride, reinterpret_cast<void *>(offsetof(PackedVertex, v0)));
    glEnableVertexAttribArray(0);
    glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, stride, reinterpret_cast<void *>(offsetof(PackedVertex, v1)));
    glEnableVertexAttribArray(1);

    meshVertexCount = static_cast<uint>(meshVertices.size());
    meshVertices.clear();
    meshVertices.shrink_to_fit();

    // Upload water mesh — same packed format.
    if (!waterMeshVertices.empty()) {
        if (waterVAO == 0)
            glGenVertexArrays(1, &waterVAO);
        if (waterVBO == 0)
            glGenBuffers(1, &waterVBO);

        glBindVertexArray(waterVAO);
        glBindBuffer(GL_ARRAY_BUFFER, waterVBO);
        glBufferData(GL_ARRAY_BUFFER, waterMeshVertices.size() * sizeof(PackedVertex), waterMeshVertices.data(), GL_STATIC_DRAW);

        glVertexAttribIPointer(0, 1, GL_UNSIGNED_INT, stride, reinterpret_cast<void *>(offsetof(PackedVertex, v0)));
        glEnableVertexAttribArray(0);
        glVertexAttribIPointer(1, 1, GL_UNSIGNED_INT, stride, reinterpret_cast<void *>(offsetof(PackedVertex, v1)));
        glEnableVertexAttribArray(1);

        waterMeshVertexCount = static_cast<uint>(waterMeshVertices.size());
    } else {
        waterMeshVertexCount = 0;
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
                        b,
                        static_cast<uint8_t>(getBiomeAt(lx, lz))
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
