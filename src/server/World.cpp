//
// Created by lucas on 7/1/25.
//

#include "World.hpp"

World::World() {
    std::mt19937 rng(time(nullptr));
    terrainParams.seed = rng();

	regionDirName = "region-" + std::to_string(terrainParams.seed);
	std::filesystem::create_directories(regionDirName);
    std::cout << "World seed: " << terrainParams.seed << std::endl;
}

World::World(int seed) {
	regionDirName = "region-" + std::to_string(seed);
	std::filesystem::create_directories(regionDirName);
    std::mt19937 rng(time(nullptr));
    terrainParams.seed = seed;
}

World::~World() {
}

std::shared_ptr<Chunk> World::getChunk(int chunkX, int chunkZ) {
    const ChunkPos key = Chunk::toKey(chunkX, chunkZ);
    auto it = chunks.find(key);
    if (it == chunks.end())
        return nullptr;
    return it->second;
}

void World::globalCoordsToLocalCoords(int &x, int &y, int &z, int globalX, int globalY, int globalZ, int &chunkX, int &chunkZ)
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

BlockType World::getBlockWorld(glm::ivec3 globalCoords)
{
	int x, y, z;
	int chunkX, chunkZ;
	globalCoordsToLocalCoords(x, y, z, globalCoords.x, globalCoords.y, globalCoords.z, chunkX, chunkZ);

	auto it = chunks.find(std::make_pair(chunkX, chunkZ));
	if (it == chunks.end()) {
		return BlockType::AIR;
	}
	std::shared_ptr<Chunk> currChunk = it->second;
	return currChunk->getBlock(x, y, z);
}

void World::setBlockWorld(glm::ivec3 globalCoords, std::optional<glm::ivec3> faceNormal, BlockType type)
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

    std::shared_ptr<Chunk> currChunk = it->second;

	updatedBlocks.push_back({glm::ivec3(targetCoords.x, targetCoords.y, targetCoords.z), type});
    currChunk->setBlock(x, y, z, type);
}

bool World::isBlockVisibleWorld(glm::ivec3 globalCoords)
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

//TODO change it. removing from memory based on player loadRadius makes no sense
void World::handleOutOfMemory(int currentChunkX, int currentChunkZ, int loadRadius) {
	if (!outOfMemory) {
		try {
			updateRegionStreaming(currentChunkX, currentChunkZ);
		} catch (std::exception &e) {
			std::cerr << "Error in updateRegionStreaming: " << e.what() << " - possibly out of memory." << std::endl;

			outOfMemory = true;
		}
	} else {
		//remove chunks to not go out of memory;
		const int unloadRadius = loadRadius + REGION_SIZE;
		std::vector<ChunkPos> toRemove;
		for (const auto& entry : chunks) {
			const int cx = entry.first.first;
			const int cz = entry.first.second;
			const int dx = cx - currentChunkX;
			const int dz = cz - currentChunkZ;
			if (dx * dx + dz * dz > unloadRadius * unloadRadius) {
				toRemove.push_back(entry.first);
			}
		}
		for (auto k : toRemove){
			chunks.erase(k);
		}
	}
}


void World::removeLoadedChunksFromPlayer(CPlayerInfo &player)
{
    int unloadRadius = player.loadRadius + 16;

    // convert player position (world coords) to chunk coords
    int playerChunkX = static_cast<int>(std::floor(player.getPosition().x / Chunk::WIDTH));
    int playerChunkZ = static_cast<int>(std::floor(player.getPosition().z / Chunk::DEPTH));

    for (auto it = player.loadedChunks.begin(); it != player.loadedChunks.end(); )
    {
        int dx = it->first - playerChunkX;
        int dz = it->second - playerChunkZ;
        int distSq = dx * dx + dz * dz;

        if (distSq >= unloadRadius * unloadRadius)
        {
            it = player.loadedChunks.erase(it); // erase returns next iterator
        }
        else
        {
            ++it;
        }
    }
}

void World::setCandidates(std::vector<std::tuple<int, int, float, float>> &candidates,
                          const CPlayerInfo &player)
{
    glm::vec2 camDir = glm::normalize(glm::vec2(player.getCameraDir().x, player.getCameraDir().z));
    float maxDist = static_cast<float>(player.loadRadius);

    // Get player’s current chunk position
    int baseChunkX = static_cast<int>(std::floor(player.getPosition().x / Chunk::WIDTH));
    int baseChunkZ = static_cast<int>(std::floor(player.getPosition().z / Chunk::DEPTH));

    for (int dx = -player.loadRadius; dx <= player.loadRadius; ++dx) {
        for (int dz = -player.loadRadius; dz <= player.loadRadius; ++dz) {
            if (dx * dx + dz * dz >= (int)maxDist * (int)maxDist)
                continue;

            int cx = baseChunkX + dx;
            int cz = baseChunkZ + dz;

            float dist = std::sqrt(static_cast<float>(dx * dx + dz * dz));
            glm::vec2 offset(dx, dz);
            float dirScore = glm::dot(glm::normalize(offset), camDir);

            candidates.emplace_back(cx, cz, dist, dirScore);
        }
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const auto& a, const auto& b) {
            float distA = std::get<2>(a), distB = std::get<2>(b);
            if (distA != distB) return distA < distB;
            return std::get<3>(a) > std::get<3>(b);
        });
}

// updates Planned Chunks AND sets chunks to send player
void World::updatePlannedChunks(CPlayerInfo &player)
{
	std::vector<std::tuple<int, int, float, float>> candidates;

	setCandidates(candidates, player);
	for (auto [cx, cz, dist, distCore] : candidates)
	{
		ChunkPos key = Chunk::toKey(cx, cz);
		std::shared_ptr<Chunk> chunk = getChunk(cx, cz);
		if (!chunk && !plannedChunks.contains(key)) { // contains is c++ 20
			plannedChunks.insert(key);
		}

		if (chunk && !player.loadedChunks.contains(key))
		{
			player.loadedChunks.insert(key);
			player.rdyChunks.push_back(key);
		}
	}
}

//TODO fix the load / saave regions with multiple players
void World::updateVisibleChunks(CPlayerInfo &player) {
    // Unload distant chunks to free memory.  Chunks beyond (loadRadius + 2)
    // in a circular distance from the camera are removed.  We copy the keys
    // to a temporary list to avoid invalidating the iterator while erasing.

	const int currentChunkX = static_cast<int>(std::floor(player.getPosition().x / Chunk::WIDTH));
	const int currentChunkZ = static_cast<int>(std::floor(player.getPosition().z / Chunk::DEPTH));

	handleOutOfMemory(currentChunkX, currentChunkZ, player.loadRadius);
	
	// std::unordered_set<ChunkPos> generatingChunks;
	uint amountOfConcurrentChunksBeingGenerated = 0;

	removeLoadedChunksFromPlayer(player);
	updatePlannedChunks(player);
	
	for (const auto& [cx, cz] : plannedChunks) {
		ChunkPos key = Chunk::toKey(cx, cz);
		std::shared_ptr<Chunk> chunk = getChunk(cx, cz);

		if (!chunk && amountOfConcurrentChunksBeingGenerated < maxConcurrentGeneration) {
			generationFutures.push_back(std::async(std::launch::async, [=, this]() {
				std::shared_ptr<Chunk> newChunk = std::make_shared<Chunk>(cx, cz, terrainParams);
				return std::make_pair(key, newChunk);
			}));
			amountOfConcurrentChunksBeingGenerated++;
		}
		else if (chunk && chunk->preGenerated && amountOfConcurrentChunksBeingGenerated < maxConcurrentGeneration) 
		{
			// generatingChunks.insert(key);
			amountOfConcurrentChunksBeingGenerated++;
			chunk->preGenerated = false;
		}
	}

	// Process a limited number of ready futures.  This spreads the cost of
	// inserting chunks into the world over multiple frames and avoids long
	// stalls while waiting for all chunks to generate at once.  We loop
	// through the futures vector, checking each for readiness with a
	// zero-duration wait. 

	// Set of chunks currently being generated asynchronously.  We use
	// ChunkKey pairs to avoid scheduling the same chunk multiple times.

	std::size_t processed = 0;
	for (auto it = generationFutures.begin(); it != generationFutures.end(); ) {
		std::future<std::pair<ChunkPos, std::shared_ptr<Chunk>>>& fut = *it;
		
		if (fut.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
			auto result = fut.get();
			// generatingChunks.insert(result.first);
			chunks[result.first] = result.second;
			plannedChunks.erase(result.first);

			it = generationFutures.erase(it);
			processed++;
		}
		else {
			it++;
		}
	}
}

// Return the number of chunks currently in the rendered list.
// std::size_t World::getRenderedChunkCount() const {
//     return renderedChunks.size();
// }

// Return the total number of chunks currently loaded in the world (in memory).
std::size_t World::getTotalChunkCount() const {
    return chunks.size();
}

void World::saveRegionsOnExit()
{
    for (auto it = loadedRegions.begin(); it != loadedRegions.end();) {
        saveRegion(it->first, it->second);
        it = loadedRegions.erase(it);
    }
}

void World::updateRegionStreaming(int currentChunkX, int currentChunkZ) {
    const int REGION_SIZE = 32;

    // Determine current region
    int regionX = floorDiv(currentChunkX, REGION_SIZE);
    int regionZ = floorDiv(currentChunkZ, REGION_SIZE);

    std::unordered_set<ChunkPos> regionsToKeep;

    // Always keep current region + 8 surrounding regions
    for (int dx = -1; dx <= 1; ++dx) {
        for (int dz = -1; dz <= 1; ++dz) {
            ChunkPos neighbor(regionX + dx, regionZ + dz);
            regionsToKeep.insert(neighbor);

            if (!loadedRegions.count(neighbor)) {
                loadRegion(neighbor.first, neighbor.second);
                loadedRegions.insert(neighbor);
            }
        }
    }

    // Unload regions that are not in the 3x3 grid
    for (auto it = loadedRegions.begin(); it != loadedRegions.end();) {
        if (!regionsToKeep.count(*it)) {
            saveRegion(it->first, it->second);
            it = loadedRegions.erase(it);
        } else {
            ++it;
        }
    }
}

//TODO handle the throws or change them to returns
void World::saveRegion(int regionX, int regionZ) {
    std::string filename = getRegionFilename(regionX, regionZ);
    std::ofstream out(filename, std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot open region file for writing: " + filename);

    // --- Write metadata ---
    RegionFileMetadata metadata;
    out.write(reinterpret_cast<const char*>(&metadata), sizeof(metadata));

    // --- Reserve header space ---
    std::vector<ChunkEntry> header(REGION_SIZE * REGION_SIZE); // all zeroed
    out.write(reinterpret_cast<const char*>(header.data()), header.size() * sizeof(ChunkEntry));

    // --- Write chunks ---
	for (int x = regionX * REGION_SIZE; x < (regionX + 1) * REGION_SIZE; x++) {
		for (int z = regionZ * REGION_SIZE; z < (regionZ + 1) * REGION_SIZE; z++)
		{
			auto it = chunks.find(Chunk::toKey(x, z));
			if (it == chunks.end()) continue ;
			
			std::streampos currPos = out.tellp();
			it->second->saveToStream(out);
			std::streampos newPos = out.tellp();

			ChunkEntry entry;
			entry.X = it->first.first;
			entry.Z = it->first.second;
			entry.offset = static_cast<std::uint32_t>(currPos);
			entry.size   = static_cast<std::uint32_t>(newPos - currPos);

			//int idx = (x % REGION_SIZE) * REGION_SIZE + z;
			int localX = x - regionX * REGION_SIZE;
			int localZ = z - regionZ * REGION_SIZE;
			int idx = localZ * REGION_SIZE + localX;

			header[idx] = entry;

			chunks.erase(it);
		}
	}


    // --- Rewrite header with correct entries ---
    out.seekp(sizeof(metadata));
    out.write(reinterpret_cast<const char*>(header.data()), header.size() * sizeof(ChunkEntry));
}


void World::loadRegion(int regionX, int regionZ) {
    std::string filename = getRegionFilename(regionX, regionZ);
    std::ifstream in(filename, std::ios::binary);
    if (!in) return ;

    // --- Read metadata ---
    RegionFileMetadata metadata;
    in.read(reinterpret_cast<char*>(&metadata), sizeof(metadata));
    if (std::strncmp(metadata.magic, "RGN1", 4) != 0)
        throw std::runtime_error("Invalid region file magic in " + filename);

    // --- Read header ---
    std::vector<ChunkEntry> header(REGION_SIZE * REGION_SIZE);
    in.read(reinterpret_cast<char*>(header.data()), header.size() * sizeof(ChunkEntry));

    // --- Load each chunk ---
    for (const auto& entry : header) {
        if (entry.size == 0 || entry.offset == 0) continue; // empty slot

        // Seek to the chunk data
        in.seekg(entry.offset);
        auto chunk = std::make_shared<Chunk>(entry.X, entry.Z, terrainParams, false);
        chunk->loadFromStream(in);

        // Insert into chunk map
        ChunkPos pos(entry.X, entry.Z);
        chunks[pos] = chunk;
    }
}

std::string World::getRegionFilename(int regionX, int regionZ) const {
	
    std::ostringstream ss;
    ss << regionDirName + "/r." << regionX << "." << regionZ << ".rg";
    return ss.str();
}

//TODO : put it on shared. maths or something and reuse it for camera and world. And maybe make it accept an std::function instead of a unique ptr?
bool World::getTargetedBlock(const CPlayerInfo &player, glm::ivec3& hitBlock, glm::ivec3& faceNormal, float maxDistance) {
    glm::vec3 rayOrigin = player.getPosition();
    glm::vec3 rayDir = glm::normalize(player.getCameraDir());

    glm::ivec3 blockPos = glm::floor(rayOrigin);

    glm::vec3 deltaDist = glm::abs(glm::vec3(1.0f) / rayDir);
    glm::ivec3 step;
    glm::vec3 sideDist;

    for (int i = 0; i < 3; ++i) {
        if (rayDir[i] < 0) {
            step[i] = -1;
            sideDist[i] = (rayOrigin[i] - blockPos[i]) * deltaDist[i];
        } else {
            step[i] = 1;
            sideDist[i] = (blockPos[i] + 1.0f - rayOrigin[i]) * deltaDist[i];
        }
    }

    float distanceTraveled = 0.0f;
    glm::ivec3 prevBlock = blockPos;

    while (distanceTraveled < maxDistance) {
        int axis;
        if (sideDist.x < sideDist.y) {
            if (sideDist.x < sideDist.z) axis = 0;
            else                         axis = 2;
        } else {
            if (sideDist.y < sideDist.z) axis = 1;
            else                         axis = 2;
        }

        blockPos[axis] += step[axis];
        sideDist[axis] += deltaDist[axis];

        // Track face direction
        faceNormal = glm::ivec3(0);
        faceNormal[axis] = -step[axis];

		distanceTraveled = glm::min(glm::min(sideDist.x, sideDist.y), sideDist.z);

        // Check if this block exists in your world
        if (isBlockVisibleWorld(blockPos)) {
            hitBlock = blockPos;
            return true;
        }
    }

    return false;
}

void World::removeTargettedBlock(const CPlayerInfo &player)
{
	glm::ivec3 blockPos, faceNormal;
	if (getTargetedBlock(player, blockPos, faceNormal))
		setBlockWorld(blockPos, std::nullopt, BlockType::AIR);
}

void World::setTargettedBlock(const CPlayerInfo &player)
{
	glm::ivec3 blockPos, faceNormal;
	if (getTargetedBlock(player, blockPos, faceNormal))
		setBlockWorld(blockPos, faceNormal, BlockType::DIRT);
}

void World::processPlayerMouseInputs(const CPlayerInfo &player, const NetPlayerMouseInputs &pkt)
{
	if (pkt.mouseButtons & IN_RIGHT_CLICK) setTargettedBlock(player);
	if (pkt.mouseButtons & IN_LEFT_CLICK) removeTargettedBlock(player);
}