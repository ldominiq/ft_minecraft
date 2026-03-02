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

bool Renderer::setBlockWorld(glm::ivec3 globalCoords, std::optional<glm::ivec3> faceNormal, BlockType type)
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
        return false;

    std::shared_ptr<ChunkRenderer> currChunk = it->second;

    currChunk->setBlock(x, y, z, type);
	currChunk->needsUpdate = true;

	// //update possible neighbour
	if (x == 0)
		currChunk->neighbourNeedUpdate[WEST] = true;
	if (x == Chunk::WIDTH - 1)
		currChunk->neighbourNeedUpdate[EAST] = true;
	if (z == 0)
		currChunk->neighbourNeedUpdate[SOUTH] = true;
	if (z == Chunk::DEPTH - 1)
		currChunk->neighbourNeedUpdate[NORTH] = true;

	return true;
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
	// Collect all chunks that need building this frame.
	std::vector<std::pair<ChunkPos, std::shared_ptr<ChunkRenderer>>> toBuild;
	for (auto [chunkX, chunkZ] : chunksToBuild) {
		std::shared_ptr<ChunkRenderer> currChunk = getChunk(chunkX, chunkZ);
		if (!currChunk) continue;
		toBuild.push_back({{chunkX, chunkZ}, currChunk});
	}

	// ── Phase 1: Compute sky-light for ALL chunks first ─────────
	// Each chunk's BFS flood-fill is self-contained (only reads its
	// own block data, never crosses chunk borders).  So these can
	// run in parallel without any race conditions.
	//
	// We do this BEFORE mesh building because buildMeshData() reads
	// the sky-light of NEIGHBOR chunks for border faces.  If we
	// computed sky-light inside buildMeshData() (like before), two
	// concurrent chunks could read each other's still-empty skyLight
	// arrays and get wrong values.  By computing all sky-light
	// first, every chunk's array is populated before any mesh build
	// tries to read it.
	{
		std::vector<std::future<void>> skyLightFutures;
		for (auto& [pos, chunk] : toBuild) {
			if (chunk->hasSkyLight())
				continue; // Already computed in receiveChunk()
			auto chunkPtr = chunk; // structured bindings can't be captured directly
			skyLightFutures.push_back(std::async(std::launch::async, [chunkPtr]() {
				chunkPtr->computeSkyLight();
			}));
		}
		// Wait for all sky-light computations to finish.
		for (auto& future : skyLightFutures) {
			future.get();
		}
	}

	// ── Phase 2: Build meshes (all skyLight arrays now valid) ────
	std::vector<std::future<ChunkPos>> meshFutures;
	for (auto& [pos, chunk] : toBuild) {
		auto cx = pos.first;
		auto cz = pos.second;
		auto chunkPtr = chunk; // structured bindings can't be captured directly
		meshFutures.push_back(std::async(std::launch::async, [chunkPtr, cx, cz]() {
			chunkPtr->buildMeshData();
			return Chunk::toKey(cx, cz);
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
		// Compute sky-light immediately so that any neighbor chunk
		// building its mesh later can read valid skyLight values
		// from this chunk, even if this chunk isn't in chunksToBuild
		// yet (e.g. it doesn't have all 4 neighbors loaded).
		// Without this, neighbors would read fallback=15 from our
		// empty skyLight array and their border faces would look
		// incorrectly sunlit.
		newChunk->computeSkyLight();
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
    glDrawArrays(GL_TRIANGLES, 0, meshVerticesSize / 10); // 10 floats per vertex
}

void Renderer::render(const std::shared_ptr<Shader> &shaderProgram) const {
	for (auto& weakChunk : renderedChunks) {
		if (auto chunk = weakChunk.lock())
		{
			if (chunk->needsUpdate)
				chunk->updateMesh();
			draw(shaderProgram, chunk->getVao(), chunk->getMeshVerticesSize());
		}
	}
}

void Renderer::onEntity(NetEntityMove &pkt, const float &lastTickClientTime)
{
	glm::vec3 position(pkt.positionX, pkt.positionY, pkt.positionZ);
	entityID ID = pkt.entityID;
	float yaw = pkt.yaw;

	auto entity = entitiesMap.find(ID);
	if (entity != entitiesMap.end())
	{
		auto ent = entity->second.lock();
		if (ent) {
			ent->prevPosition = ent->nextPosition;
			ent->nextPosition = position;
			ent->yaw = yaw;
			ent->positionUpdated = true;
			ent->lastTickClientTime = lastTickClientTime;
			if (pkt.type == static_cast<uint16_t>(-1))
			{
				ent->removed = true;
				if (pkt.eEntityType == EEntityTypes::LIVING_ENTITIES)
				{
					livingEntities.erase(
						std::remove_if(livingEntities.begin(), livingEntities.end(),
							[ID](const std::shared_ptr<Entity>& e){ return e->getID() == ID; }),
						livingEntities.end()
					);
				}
			}
		}
		else if (entity->second.expired()) {
			entitiesMap.erase(ID);
		}
	}
	else
	{
		if (pkt.eEntityType == EEntityTypes::ITEMS)
		{
			BlockType type = static_cast<BlockType>(pkt.type);
			auto entityPtr = std::make_shared<ItemPropEntity>(position, yaw, type, ID);
			entityPtr->lastTickClientTime = lastTickClientTime;
			itemEntities.push_back(entityPtr);
			entitiesMap[ID] = entityPtr;
		}
		else if (pkt.eEntityType == EEntityTypes::LIVING_ENTITIES)
		{
			LivingEntityType type = static_cast<LivingEntityType>(pkt.type);
			std::shared_ptr<IClientEntity> entityPtr;
			switch (type)
			{
				case PLAYER:
					entityPtr = std::make_shared<ClientPlayer>(position, yaw, ID);
					break;
				case CREEPER:
					entityPtr = std::make_shared<ClientCreeper>(position, yaw, ID);
					break;
				default:
					std::cout << "ERROR ERROR MAYDAY WE GOT A PROBLEM" << std::endl;
					return;
			}

			livingEntitiesManager.add(entityPtr);
			// Convert to shared_ptr<LivingEntity> safely
			std::shared_ptr<LivingEntity> le = static_cast<std::shared_ptr<LivingEntity>>(entityPtr);
			livingEntities.push_back(le);
			entitiesMap[ID] = le;
		}
	}
}

void Renderer::drawCharacters(const glm::mat4 &projection, const glm::mat4 &view, const float deltatime)
{
	livingEntitiesManager.draw(projection, view, deltatime);
}

void Renderer::renderWater() const {
	glDisable(GL_CULL_FACE);
    for (const auto& weakChunk : renderedChunks) {
        if (auto chunk = weakChunk.lock()) {
            if (chunk->getWaterMeshVerticesSize() > 0) {
                glBindVertexArray(chunk->getWaterVao());
                glDrawArrays(GL_TRIANGLES, 0, chunk->getWaterMeshVerticesSize() / 10);
            }
        }
    }
	glEnable(GL_CULL_FACE);
}