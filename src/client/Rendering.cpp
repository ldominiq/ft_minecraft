#include "Rendering.hpp"


void Rendering::globalCoordsToLocalCoords(int &x, int &y, int &z, int globalX, int globalY, int globalZ, int &chunkX, int &chunkZ)
{
	x = (globalX % Chunk::WIDTH + Chunk::WIDTH) % Chunk::WIDTH;
	z = (globalZ % Chunk::DEPTH + Chunk::DEPTH) % Chunk::DEPTH;
	y = globalY;

	chunkX = globalX / Chunk::WIDTH;
	if (globalX < 0 && globalX % Chunk::WIDTH != 0)
		chunkX--;

	chunkZ = globalZ / Chunk::DEPTH;
	if (globalZ < 0 && globalZ % Chunk::DEPTH != 0)
		chunkZ--;
}

// BlockType Rendering::getBlockWorld(glm::ivec3 globalCoords)
// {
// 	int x, y, z;
// 	int chunkX, chunkZ;
// 	globalCoordsToLocalCoords(x, y, z, globalCoords.x, globalCoords.y, globalCoords.z, chunkX, chunkZ);

// 	auto it = chunks.find(std::make_pair(chunkX, chunkZ));
// 	if (it == chunks.end()) {
// 		return BlockType::AIR;
// 	}
// 	std::shared_ptr<Chunk> currChunk = it->second;
// 	return currChunk->getBlock(x, y, z);
// }

void Rendering::setBlockWorld(glm::vec3 &targetCoords, BlockType type)
{
    int x, y, z;
    int chunkX, chunkZ;
    globalCoordsToLocalCoords(x, y, z, 
        targetCoords.x, targetCoords.y, targetCoords.z, 
        chunkX, chunkZ);

    auto it = chunks.find(std::make_pair(chunkX, chunkZ));
    if (it == chunks.end())
        return;

    std::shared_ptr<Chunk> currChunk = it->second;

    currChunk->setBlock(x, y, z, type);
	currChunk->buildMesh();

	// //update possible neighbour
	if (x == 0) {
		if (auto westChunk = currChunk->getAdjacentChunks()[WEST].lock()) {
			westChunk->buildMesh();
		}
	}
	if (x == Chunk::WIDTH - 1) {
		if (auto eastChunk = currChunk->getAdjacentChunks()[EAST].lock()) {
			eastChunk->buildMesh();
		}
	}
	if (z == 0) {
		if (auto southChunk = currChunk->getAdjacentChunks()[SOUTH].lock()) {
			southChunk->buildMesh();
		}
	}
	if (z == Chunk::DEPTH - 1) {
		if (auto northChunk = currChunk->getAdjacentChunks()[NORTH].lock()) {
			northChunk->buildMesh();
		}
	}
}

bool Rendering::isBlockVisibleWorld(glm::ivec3 globalCoords)
{
	int x, y, z;
	int chunkX, chunkZ;
	globalCoordsToLocalCoords(x, y, z, globalCoords.x, globalCoords.y, globalCoords.z, chunkX, chunkZ);

	auto it = chunks.find(std::make_pair(chunkX, chunkZ));
	if (it == chunks.end()) {
		return false;
	}

	std::shared_ptr<Chunk> currChunk = it->second;
	return currChunk->isBlockVisible(glm::vec3(x, y ,z));
}

std::shared_ptr<Chunk> Rendering::getChunk(int chunkX, int chunkZ) {
    const ChunkPos key = Chunk::toKey(chunkX, chunkZ);
    auto it = chunks.find(key);
    if (it == chunks.end())
        return nullptr;
    return it->second;
}

void Rendering::linkNeighbors(int chunkX, int chunkZ, std::shared_ptr<Chunk> &chunk) {

    const int dirX[] = { 0, 0, 1, -1 };
    const int dirZ[] = { 1, -1, 0, 0 };
    const int opp[]  = { SOUTH, NORTH, WEST, EAST };

    for (int dir = 0; dir < 4; ++dir) {
        int nx = chunkX + dirX[dir];
        int nz = chunkZ + dirZ[dir];

        std::shared_ptr<Chunk> neighbor = getChunk(nx, nz);

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

std::vector<std::weak_ptr<Chunk>> Rendering::getRenderedChunks()
{
	return renderedChunks;
}

void Rendering::updateChunk(const NetModifiedBlockData &pkt)
{
	glm::vec3 targetCoords = glm::vec3(pkt.x, pkt.y, pkt.z);
	setBlockWorld(targetCoords, static_cast<BlockType>(pkt.blockType));
}

void Rendering::buildChunks()
{
	std::vector<std::future<ChunkPos>> meshFutures;

	for (auto [chunkX, chunkZ] : chunksToBuild) {
		std::shared_ptr<Chunk> currChunk = getChunk(chunkX, chunkZ);
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
void Rendering::organizeChunks(const std::pair<int, int> pos)
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

void Rendering::prepareChunk(const NetChunkHeader& pkt) {
	chunkData data;
	data.compressedSize = pkt.compressedSize;
	data.uncompressedSize = pkt.uncompressedSize;
	data.chunkBuffer.reserve(pkt.compressedSize);
	chunksData[std::make_pair(pkt.X, pkt.Z)] = data;
}

// TODO : Make this function great. could be fucked up packet > MAXLINE and 1 packet gets lost
void Rendering::receiveChunk(const NetChunkData& pkt) {
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

        std::shared_ptr<Chunk> newChunk = std::make_shared<Chunk>(iss);
		linkNeighbors(pkt.X, pkt.Z, newChunk);
		chunks[{pkt.X, pkt.Z}] = newChunk;
		// newChunk->buildMesh();
        data->second.chunkBuffer.clear();
    }
}

//I dislike having VAO here. TODO : MAYBE MAYBE change it
void Rendering::draw(const std::shared_ptr<Shader>& shader, const GLuint &VAO, const uint &meshVerticesSize) const {
    shader->use();
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, meshVerticesSize / 9);
}

void Rendering::render(const std::shared_ptr<Shader> &shaderProgram) const {
	for (auto& weakChunk : renderedChunks) {
		if (auto chunk = weakChunk.lock())
			draw(shaderProgram, chunk->getVao(), chunk->getMeshVerticesSize());
	}
}