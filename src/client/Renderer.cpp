#include "Renderer.hpp"

void Renderer::linkNeighbors(int chunkX, int chunkZ, std::shared_ptr<ChunkRenderer> &chunk) {

    const int dirX[] = { 0, 0, 1, -1 };
    const int dirZ[] = { 1, -1, 0, 0 };
    const int opp[]  = { SOUTH, NORTH, WEST, EAST };

    for (int dir = 0; dir < 4; ++dir) {
        int nx = chunkX + dirX[dir];
        int nz = chunkZ + dirZ[dir];

        std::shared_ptr<ChunkRenderer> neighbor = getChunk(nx, nz);

        chunk->setAdjacentChunks(static_cast<Direction>(dir), neighbor);
		if (chunk->hasAllAdjacentChunkLoaded())
			chunksToBuild.insert(Chunk::toKey(chunkX, chunkZ));
        if (neighbor) {
            neighbor->setAdjacentChunks(opp[dir], chunk);

            if (neighbor->hasAllAdjacentChunkLoaded()) {
				chunksToBuild.insert(Chunk::toKey(nx, nz));
            }
        }
    }
}

void Renderer::setBlockWorld(glm::ivec3 globalCoords, std::optional<glm::ivec3> faceNormal, BlockType type)
{
    // Offset the global coordinates in the direction of the face normal
    glm::ivec3 targetCoords = globalCoords;
    if (faceNormal.has_value()) {
        targetCoords += *faceNormal;
    }

    int x, y, z;
    int chunkX, chunkZ;
    globalCoordsToLocalCoords(x, y, z, 
        targetCoords.x, targetCoords.y, targetCoords.z, 
        chunkX, chunkZ);

    auto it = chunks.find(std::make_pair(chunkX, chunkZ));
    if (it == chunks.end())
        return;

    std::shared_ptr<ChunkRenderer> currChunk = it->second;

    currChunk->setBlock(x, y, z, type);
	currChunk->buildMesh();

	// //update possible neighbour
	if (x == 0) {
		if (auto westChunkBase = currChunk->getAdjacentChunks()[WEST].lock()) {
			if (auto westChunk = std::dynamic_pointer_cast<ChunkRenderer>(westChunkBase)) {
				westChunk->buildMesh();
			}
		}
	}

	if (x == Chunk::WIDTH - 1) {
		if (auto eastChunkBase = currChunk->getAdjacentChunks()[EAST].lock()) {
			if (auto eastChunk = std::dynamic_pointer_cast<ChunkRenderer>(eastChunkBase)) {
				eastChunk->buildMesh();
			}
		}
	}

	if (z == 0) {
		if (auto southChunkBase = currChunk->getAdjacentChunks()[SOUTH].lock()) {
			if (auto southChunk = std::dynamic_pointer_cast<ChunkRenderer>(southChunkBase)) {
				southChunk->buildMesh();
			}
		}
	}

	if (z == Chunk::DEPTH - 1) {
		if (auto northChunkBase = currChunk->getAdjacentChunks()[NORTH].lock()) {
			if (auto northChunk = std::dynamic_pointer_cast<ChunkRenderer>(northChunkBase)) {
				northChunk->buildMesh();
			}
		}
	}
}

std::vector<std::weak_ptr<ChunkRenderer>> Renderer::getRenderedChunks()
{
	return renderedChunks;
}

void Renderer::updateChunk(const NetModifiedBlockData &pkt)
{
	glm::vec3 targetCoords = glm::vec3(pkt.x, pkt.y, pkt.z);
	setBlockWorld(targetCoords, std::nullopt, static_cast<BlockType>(pkt.blockType));
}

void Renderer::buildChunks()
{
	std::vector<std::future<ChunkPos>> meshFutures;

	for (auto [chunkX, chunkZ] : chunksToBuild) {
		std::shared_ptr<ChunkRenderer> currChunk = getChunk(chunkX, chunkZ);
		meshFutures.push_back(std::async(std::launch::async, [chunkX, chunkZ, currChunk]() {
			currChunk->buildMeshData();
			return Chunk::toKey(chunkX, chunkZ);
		}));
	}

	for (auto it = meshFutures.begin(); it != meshFutures.end();) {
		ChunkPos pos = it->get();
		auto chunk = getChunk(pos.first, pos.second);
		if (chunk) {
			chunk->uploadMesh();
			chunks[{pos.first, pos.second}] = chunk;
		}
		it = meshFutures.erase(it);
	}
	chunksToBuild.clear();
}

//sets rendered chunks and unloads far away chunks
void Renderer::organizeChunks(const std::pair<int, int> pos)
{
    // Clear renderedChunks first
    renderedChunks.clear();

    for (auto it = chunks.begin(); it != chunks.end(); )
    {
        const ChunkPos& chunkPos = it->first;
        auto& chunkPtr = it->second;

        // Compute squared distance between chunk coordinates
        int dx = chunkPos.first - pos.first;
        int dz = chunkPos.second - pos.second;
        int distSq = dx * dx + dz * dz;

        if (distSq <= loadRadius * loadRadius)
        {
            // Inside load radius -> render
            renderedChunks.push_back(chunkPtr);
            ++it;
        }
        else if (distSq >= unloadRadius * unloadRadius)
        {
            // Outside unload radius -> remove chunk
            it = chunks.erase(it);
        }
        else
        {
            // In between -> keep chunk loaded but not rendered
            ++it;
        }
    }
}

void Renderer::prepareChunk(const NetChunkHeader& pkt) {
	chunkData data;
	data.compressedSize = pkt.compressedSize;
	data.uncompressedSize = pkt.uncompressedSize;
	data.chunkBuffer.reserve(pkt.compressedSize);
	chunksData[std::make_pair(pkt.X, pkt.Z)] = data;
}

// TODO : Make this function great. could be fucked up packet > MAXLINE and 1 packet gets lost
void Renderer::receiveChunk(const NetChunkData& pkt) {
	auto data = chunksData.find(std::make_pair(pkt.X, pkt.Z));
	if (data == chunksData.end()) // Will never not find an element because of chunk 0, 0. And X and Z initialise to 0. TODO : fix this
		return ;

	data->second.chunkBuffer.insert(data->second.chunkBuffer.end(), pkt.data.begin(), pkt.data.end());

    // If last packet for this chunk (we might need a "final packet" flag later)
    if ((pkt.flags & PacketFlags::FinalChunk) != PacketFlags::None) { // define this flag
        std::vector<uint8_t>& compressed = data->second.chunkBuffer;
        // Decompress
        std::vector<uint8_t> decompressed(data->second.uncompressedSize);
        size_t res = ZSTD_decompress(decompressed.data(), decompressed.size(), compressed.data(), compressed.size()); // TODO : maybe use decompres with a dict. would be better
		if (ZSTD_isError(res)) {
			std::cerr << "Decompression failed: " 
					<< ZSTD_getErrorName(res) << "\n"
					<< "Expected size: " << data->second.uncompressedSize
					<< ", got: " << res << "\n"
					<< "Compressed size: " << compressed.size() << "\n";
			return;
		}

        // Deserialize into BitPackedArray
        std::istringstream iss(std::string(decompressed.begin(), decompressed.end()), std::ios::binary);

        std::shared_ptr<ChunkRenderer> newChunk = std::make_shared<ChunkRenderer>(iss);
		linkNeighbors(pkt.X, pkt.Z, newChunk);
		chunks[{pkt.X, pkt.Z}] = newChunk;
		// newChunk->buildMesh();
        data->second.chunkBuffer.clear();
    }
}

//I dislike having VAO here. TODO : MAYBE MAYBE change it
void Renderer::draw(const std::shared_ptr<Shader>& shader, const GLuint &VAO, const uint &meshVerticesSize) const {
    shader->use();
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, meshVerticesSize / 9);
}

void Renderer::render(const std::shared_ptr<Shader> &shaderProgram) const {
	for (auto& weakChunk : renderedChunks) {
		if (auto chunk = weakChunk.lock())
			draw(shaderProgram, chunk->getVao(), chunk->getMeshVerticesSize());
	}
}