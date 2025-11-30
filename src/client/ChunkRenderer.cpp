
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
                    addFace(meshVertices, x, y, z, originX, originZ, blockTypeVector[(x) + WIDTH * ((y) + HEIGHT * (z))], 0, true); //addFace(x, y, z, 0);

                // BACK (-Z)
                if (getBlockOrNeighbor(x, y, z, 0, 0, -1, SOUTH) == BlockType::AIR)
                    addFace(meshVertices, x, y, z, originX, originZ, blockTypeVector[(x) + WIDTH * ((y) + HEIGHT * (z))], 1, true);

                // TOP (+Y) – no vertical neighbor chunks
                if (y == HEIGHT - 1 || getBlockOrNeighbor(x, y, z, 0, +1, 0, NONE) == BlockType::AIR)
                    addFace(meshVertices, x, y, z, originX, originZ, blockTypeVector[(x) + WIDTH * ((y) + HEIGHT * (z))], 2, true);

                // BOTTOM (-Y)
                if (y == 0 || getBlockOrNeighbor(x, y, z, 0, -1, 0, NONE) == BlockType::AIR)
                    addFace(meshVertices, x, y, z, originX, originZ, blockTypeVector[(x) + WIDTH * ((y) + HEIGHT * (z))], 3, true);

                // RIGHT (+X)
                if (getBlockOrNeighbor(x, y, z, +1, 0, 0, EAST) == BlockType::AIR)
                    addFace(meshVertices, x, y, z, originX, originZ, blockTypeVector[(x) + WIDTH * ((y) + HEIGHT * (z))], 4, true);

                // LEFT (-X)
                if (getBlockOrNeighbor(x, y, z, -1, 0, 0, WEST) == BlockType::AIR)
                    addFace(meshVertices, x, y, z, originX, originZ, blockTypeVector[(x) + WIDTH * ((y) + HEIGHT * (z))], 5, true);
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
