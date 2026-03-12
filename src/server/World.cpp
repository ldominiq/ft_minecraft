//
// Created by lucas on 7/1/25.
//

#include "World.hpp"

World::World() {
    std::mt19937 rng(time(nullptr));
    terrainParams.seed = rng();

	std::string regionsDirName = "Regions/";
	regionDirName = regionsDirName + "region-" + std::to_string(terrainParams.seed);
	if (SAVES_ACTIVE)
	{
		std::filesystem::create_directories(regionsDirName);
		std::filesystem::create_directories(regionDirName);
	}
    std::cout << "World seed: " << terrainParams.seed << std::endl;

	CircularGrid = buildCircularOffsets(MAX_RADIUS);
}

World::World(int seed) {
	std::cout << "World seed: " << seed << std::endl;
	std::string regionsDirName = "Regions/";
	regionDirName = regionsDirName + "region-" + std::to_string(seed);
	if (SAVES_ACTIVE)
	{
		std::filesystem::create_directories(regionsDirName);
		std::filesystem::create_directories(regionDirName);
	}
    terrainParams.seed = seed;

	CircularGrid = buildCircularOffsets(MAX_RADIUS);
}

World::~World() {
}

// helper to write PPM
static void saveHeightmapPPM(const std::string &path, const std::vector<float> &heightmap, int w, int h) {
    float minH = std::numeric_limits<float>::infinity();
    float maxH = -std::numeric_limits<float>::infinity();
    for (float v : heightmap) { minH = std::min(minH, v); maxH = std::max(maxH, v); }
    // avoid divide by zero
    float range = (maxH - minH);
    if (range < 1e-6f) range = 1.0f;

    std::ofstream f(path, std::ios::binary);
    f << "P6\n" << w << " " << h << "\n255\n";
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            float v = heightmap[i + j * w];
            unsigned char c = static_cast<unsigned char>(glm::clamp((v - minH) / range, 0.0f, 1.0f) * 255.0f);
            unsigned char col[3] = { c, c, c };
            f.write(reinterpret_cast<char*>(col), 3);
        }
    }
    f.close();
}

// Dumps a world-sized heightmap by sampling noise without generating chunks
// centerChunkX/Z: center of the dump in chunk coordinates
// chunksX/chunksZ: number of chunks across and down (total image = chunksX*Chunk::WIDTH)
// downsample: sample every N world-voxels to reduce output resolution and cost
void World::dumpHeightmap(int centerChunkX, int centerChunkZ, int chunksX, int chunksZ, int downsample, int image) const {
    if (downsample < 1) downsample = 1;

    // compute world bounds in world-block coordinates
    constexpr int chunkW = Chunk::WIDTH;
    constexpr int chunkD = Chunk::DEPTH;
    const long worldW = static_cast<long>(chunksX) * chunkW;
    const long worldD = static_cast<long>(chunksZ) * chunkD;

    // top-left world coordinate (block space)
    const long startX = (static_cast<long>(centerChunkX) - chunksX/2) * chunkW;
    const long startZ = (static_cast<long>(centerChunkZ) - chunksZ/2) * chunkD;

    // output resolution after downsampling
    const int outW = static_cast<int>((worldW + downsample - 1) / downsample);
    const int outH = static_cast<int>((worldD + downsample - 1) / downsample);
    std::vector<float> img(outW * outH);
    std::vector<float> imgCont(outW * outH);
    std::vector<float> imgEro(outW * outH);
    std::vector<float> imgPV(outW * outH);
    std::vector<float> imgHumidity(outW * outH);
    std::vector<float> imgTemperature(outW * outH);

    for (int oz = 0, wz = 0; wz < outH; ++wz, oz += downsample) {
        for (int ox = 0, wx = 0; wx < outW; ++wx, ox += downsample) {
            const auto worldX = static_cast<float>(startX + ox);
            const auto worldZ = static_cast<float>(startZ + oz);

        	const float continentalness = ChunkGeneration::getContinentalness(terrainParams, worldX, worldZ);
        	const float erosion = ChunkGeneration::getErosion(terrainParams, worldX, worldZ);
        	const float pv = ChunkGeneration::getPV(terrainParams, worldX, worldZ);

            if (image == 0) {

                const int surfaceY = ChunkGeneration::computeTerrainHeight(terrainParams, worldX, worldZ);
                
                img[wx + wz * outW] = surfaceY;
                // imgCont[wx + wz * outW] = baseHeight;
                // imgEro[wx + wz * outW] = erosionDelta;
                // imgPV[wx + wz * outW] = pvFactor;
            } else if (image == 1) {
                imgCont[wx + wz * outW] = continentalness;
                imgEro[wx + wz * outW] = erosion;
                imgPV[wx + wz * outW] = pv;
            	imgHumidity[wx + wz * outW] = ChunkGeneration::getHumidity(terrainParams, worldX, worldZ);
            	imgTemperature[wx + wz * outW] = ChunkGeneration::getTemperature(terrainParams, worldX, worldZ);

            }
            
        }
    }
    if (image == 0) {
        saveHeightmapPPM("heightmap.ppm", img, outW, outH);
     //    saveHeightmapPPM("continentalnessHM.ppm", imgCont, outW, outH);
     //    saveHeightmapPPM("erosionHM.ppm", imgEro, outW, outH);
    	// saveHeightmapPPM("pvHM.ppm", imgPV, outW, outH);
    } else if (image == 1) {
        saveHeightmapPPM("continentalnessNoise.ppm", imgCont, outW, outH);
        saveHeightmapPPM("erosionNoise.ppm", imgEro, outW, outH);
    	saveHeightmapPPM("pvNoise.ppm", imgPV, outW, outH);
    	saveHeightmapPPM("humidNoise.ppm", imgHumidity, outW, outH);
    	saveHeightmapPPM("tempNoise.ppm", imgTemperature, outW, outH);
    }
}

// Dump a color-coded biome map as a PPM. Each column maps to a pixel.
void World::dumpBiomeMap(int centerChunkX, int centerChunkZ, int chunksX, int chunksZ, int downsample) {
    // Validate inputs
    if (downsample <= 0) downsample = 1;
    if (chunksX <= 0 || chunksZ <= 0) return;

    // Compute image dimensions with ceil-division to avoid 0 and off-by-one
    const int chunkW = Chunk::WIDTH;
    const int chunkD = Chunk::DEPTH;
    const int worldW = chunksX * chunkW;
    const int worldH = chunksZ * chunkD;
    const int imgW = (worldW + downsample - 1) / downsample;
    const int imgH = (worldH + downsample - 1) / downsample;

    // Guard against absurd sizes
    if (imgW <= 0 || imgH <= 0) return;

    // Pre-size output and only index inside [0, size)
    std::vector<glm::u8vec3> pixels;
    pixels.resize(static_cast<size_t>(imgW) * static_cast<size_t>(imgH));

    // Compute the starting world position (top-left) in block coords
    const int startChunkX = centerChunkX - chunksX / 2;
    const int startChunkZ = centerChunkZ - chunksZ / 2;
    const int startWorldX = startChunkX * chunkW;
    const int startWorldZ = startChunkZ * chunkD;

    // Iterate over world-space in steps of downsample and fill pixels
    for (int z = 0; z < worldH; z += downsample) {
        // Compute y (row) index using ceil-division consistent with imgH
        const int py = z / downsample;
        if (py >= imgH) break; // safety

        for (int x = 0; x < worldW; x += downsample) {
            const int px = x / downsample;
            if (px >= imgW) break; // safety

            // Map to world coordinates
            const float wx = static_cast<float>(startWorldX + x);
            const float wz = static_cast<float>(startWorldZ + z);

            // Sample your biome function (replace with your logic)
            const int height = ChunkGeneration::computeTerrainHeight(terrainParams, wx, wz);
            const BiomeType biome = ChunkGeneration::computeBiome(terrainParams, wx, wz, height);

            glm::u8vec3 color;
            switch (biome) {
                case BiomeType::PLAINS:  color = { 80, 200, 120 }; break;
                case BiomeType::DESERT:  color = { 210, 180, 80 }; break;
                case BiomeType::FOREST:  color = { 40, 160, 60 }; break;
                case BiomeType::TUNDRA:  color = { 200, 220, 230 }; break;
                case BiomeType::SWAMP:   color = { 150, 140, 45 }; break;
                case BiomeType::OCEAN:   color = { 40, 60, 160 }; break;
            	case BiomeType::MOUNTAIN: color = { 100, 100, 100 }; break;
                default:                  color = { 255, 0, 255 }; break;
            }

            const size_t idx = static_cast<size_t>(py) * static_cast<size_t>(imgW)
                             + static_cast<size_t>(px);
            if (idx < pixels.size()) {
                pixels[idx] = color;
            }
        }
    }


	std::ofstream out("biome.ppm", std::ios::binary);
	out << "P6\n" << imgW << " " << imgH << "\n255\n";
	for (size_t i = 0; i < pixels.size(); ++i) {
		const glm::u8vec3 c = pixels[i];
		char rgb[3] = { static_cast<char>(c.r),
						static_cast<char>(c.g),
						static_cast<char>(c.b) };
		out.write(rgb, 3);
	}
}

void World::linkNeighbors(int chunkX, int chunkZ, std::shared_ptr<ChunkGeneration> &chunk) {

    const int dirX[] = { 0, 0, 1, -1 };
    const int dirZ[] = { 1, -1, 0, 0 };
    const int opp[]  = { SOUTH, NORTH, WEST, EAST };

    for (int dir = 0; dir < 4; ++dir) {
        int nx = chunkX + dirX[dir];
        int nz = chunkZ + dirZ[dir];

        std::shared_ptr<ChunkGeneration> neighbor = getChunk(nx, nz);

        chunk->setAdjacentChunks(static_cast<Direction>(dir), neighbor);
        if (neighbor) {
            neighbor->setAdjacentChunks(opp[dir], chunk);
        }
    }
}

// std::vector<ChunkPos> CircularGrid
std::vector<ChunkPos> World::buildCircularOffsets(int radius)
{
    std::vector<ChunkPos> v;

    for (int x = -radius; x <= radius; x++)
    {
        for (int y = -radius; y <= radius; y++)
        {
            if (x*x + y*y <= radius*radius)
            {
                v.push_back({x,y});
            }
        }
    }

    std::sort(v.begin(), v.end(), [](const ChunkPos& a, const ChunkPos& b)
    {
        int da = a.first*a.first + a.second*a.second;
        int db = b.first*b.first + b.second*b.second;
        return da < db;
    });

    return v;
}

ChunkPos World::findNextChunk(CPlayerInfo& player)
{
	int baseChunkX = static_cast<int>(std::floor(player.movement->getPosition().x / Chunk::WIDTH));
	int baseChunkZ = static_cast<int>(std::floor(player.movement->getPosition().z / Chunk::DEPTH));

	auto& known = PlayerKnownChunks[player.id];
	int r2 = player.movement->loadRadius * player.movement->loadRadius;

	for (const ChunkPos& off : CircularGrid)
	{
		if (off.first * off.first + off.second * off.second > r2)
			break;

		ChunkPos c{baseChunkX + off.first, baseChunkZ + off.second};

		if (!known.contains(c) && !plannedChunks.contains(c))
			return c;
	}

	return INVALID_CHUNK;
}

void World::unloadPlayerKnownChunks(CPlayerInfo &player)
{
	int unloadRadius = player.movement->loadRadius * 4;
	int playerChunkX = static_cast<int>(std::floor(player.movement->getPosition().x / Chunk::WIDTH));
	int playerChunkZ = static_cast<int>(std::floor(player.movement->getPosition().z / Chunk::DEPTH));

    for (auto it = PlayerKnownChunks[player.id].begin(); it != PlayerKnownChunks[player.id].end(); )
    {
        const ChunkPos& chunkPos = *it;

        // Compute squared distance between chunk coordinates
        int dx = chunkPos.first - playerChunkX;
        int dz = chunkPos.second - playerChunkZ;
        int distSq = dx * dx + dz * dz;

        if (distSq > unloadRadius * unloadRadius)
            it = PlayerKnownChunks[player.id].erase(it);
        else
            ++it;
    }
}

void World::updateVisibleChunks(CPlayerInfo &player)
{
	// generate chunks in parallel
	for(int i = 0; i < maxConcurrentGenerationPerPlayer && plannedChunks.size() < maxConcurrentGeneration; i++)
	{
		ChunkPos bestChunk = findNextChunk(player);
		if (bestChunk == INVALID_CHUNK) break;

		PlayerKnownChunks[player.id].insert(bestChunk);

		if (chunks.find(bestChunk) != chunks.end())
			rdyChunks.push_back(bestChunk);
		else
		{	
			plannedChunks.insert(bestChunk);
			chunkJobs[bestChunk] = std::async(std::launch::async, [this, bestChunk]() {
				return std::make_shared<ChunkGeneration>(bestChunk.first, bestChunk.second, terrainParams);
			});
		}
	}

	unloadPlayerKnownChunks(player);
}

//Catch Chunk when generated
void World::updateRdyChunks()
{
	for (auto it = chunkJobs.begin(); it != chunkJobs.end(); )
	{
		auto& future = it->second;
		if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
		{
			std::shared_ptr<ChunkGeneration> chunk = future.get();
			chunks[chunk->getPos()] = chunk;
			linkNeighbors(chunk->getPos().first, chunk->getPos().second, chunk);

			rdyChunks.push_back(chunk->getPos());
			plannedChunks.erase(it->first);
			it = chunkJobs.erase(it);
		}
		else
			++it;
	}
}

void World::updatePlayerRdyChunks(CPlayerInfo &player)
{
	for (const auto& chunkPos : rdyChunks)
	{
		if (PlayerKnownChunks[player.id].find(chunkPos) != PlayerKnownChunks[player.id].end())
			player.rdyChunks.push_back(chunkPos);
	}
}

// finds the shortest (at most 4 blocks away) path to fall
// This is being done iteratively
//todo: if perf is an issue also add a check for depth <= currPropagation
std::vector<s_waterPath> World::findShortestWaterPath(const glm::ivec3 &initialBlockPos)
{
	std::vector<s_waterPath> furthestsBlocksPath;
	std::vector<s_waterPath> newFurthestsBlocksPath;
	std::unordered_set<glm::ivec3> visited;
	std::vector<s_waterPath> finalPaths;

	glm::ivec3 down(0, -1, 0);

	static const glm::ivec3 directions[5] = {
		{ 0, -1,  0}, // Down
		{-1,  0,  0}, // Left
		{ 1,  0,  0}, // Right
		{ 0,  0,  1}, // Front
		{ 0,  0, -1}  // Back
	};

	int depth = 0;
	bool pathFound = false;

	furthestsBlocksPath.push_back({});

	while (depth <= 4 && !pathFound)
	{
		for (auto &currPath : furthestsBlocksPath)
		{
			for (auto &dir : directions)
			{
				s_waterPath newPath = currPath;
				newPath.currBlockPos += dir;
				newPath.currPath.push_back(dir);

				glm::ivec3 newPosition(newPath.currBlockPos + initialBlockPos);
				if (visited.contains(newPosition)) continue;
				visited.insert(newPosition);

				BlockType type = getBlockWorld(newPosition);

				if (type == BlockType::AIR)
				{
					if (dir == down)
					{
						pathFound = true;
						finalPaths.push_back(newPath);
					}
					else
						newFurthestsBlocksPath.push_back(newPath);
				}
			}
		}
		furthestsBlocksPath = std::move(newFurthestsBlocksPath);
		depth++;
	}

	return finalPaths;
}

std::vector<std::shared_ptr<s_liquid>> World::waterFlowTowardsShortestPath(const glm::ivec3 &initialBlockPos, const std::shared_ptr<s_liquid> &liquid, const std::vector<s_waterPath> &paths)
{
	std::unordered_map<glm::ivec3, std::shared_ptr<s_liquid>> newLiquids;

	glm::ivec3 down(0, -1, 0);

	for (auto &path : paths)
	{
		if (path.currPath.empty()) continue ;

		glm::ivec3 pos = path.currPath.front();
		glm::ivec3 position = pos + initialBlockPos;

		if (getBlockWorld(position) != BlockType::AIR) continue ;

		int newLiquidPropagationValue = liquid->currPropagation - 1;
		if (path.currPath.size() == 2 && path.currPath.back() == down) // if propagation goes to 0 but last is down. make sure it goes down and doesn't keep floating
			newLiquidPropagationValue = liquid->currPropagation;
		if (pos == down)
			newLiquidPropagationValue = s_liquid{}.currPropagation;

		s_waterPath newWaterPath;
		newWaterPath.currPath = std::vector<glm::ivec3>(
			path.currPath.begin() + 1, path.currPath.end());

		auto it = liquidsManager.liquids.find(position);
		if (it == liquidsManager.liquids.end())
		{
			std::shared_ptr<s_liquid> newLiquidPtr = std::make_shared<s_liquid>();
			newLiquidPtr->currPropagation = newLiquidPropagationValue;
			newLiquidPtr->liquidType = BlockType::WATER;
			newLiquidPtr->position = position;
			newLiquidPtr->source = liquid;
			if (!newWaterPath.currPath.empty())
				newLiquidPtr->currentPaths.push_back(newWaterPath);

			newLiquids[position] = newLiquidPtr;
		}
		else // if liquid already exists just add the path to it
			it->second->currentPaths.push_back(newWaterPath);
	}

	std::vector<std::shared_ptr<s_liquid>> newLiquidsVector;
	newLiquidsVector.reserve(newLiquids.size());
	for (auto &[pos, liquid] : newLiquids)
		newLiquidsVector.push_back(liquid);

	return newLiquidsVector;
}

// update liquids. TODO: maybe separate liquid creation and deletion. current flow could end up MAYBE creating issues?
void World::updateLiquids()
{
	glm::ivec3 down(0, -1, 0);

	static const glm::ivec3 directions[5] = {
		{ 0, -1,  0}, // Down
		{-1,  0,  0}, // Left
		{ 1,  0,  0}, // Right
		{ 0,  0,  1}, // Front
		{ 0,  0, -1}  // Back
	};

	std::unordered_set<glm::ivec3> visitedPositions;
	std::vector<glm::ivec3> liquidsToRemove;

	std::vector<std::shared_ptr<s_liquid>> newLiquids;
	for (auto &[pos, liquid] : liquidsManager.liquidsToUpdate)
	{
		if (liquid->source.expired())
		{
			liquidsToRemove.push_back(pos);
			continue ;
		}

		if (liquid->currentPaths.empty())
		{
			std::vector<s_waterPath> paths = findShortestWaterPath(pos);
			if (!paths.empty())
				liquid->currentPaths = paths;
		}
		if (!liquid->currentPaths.empty())
		{
			std::vector<std::shared_ptr<s_liquid>> extraLiquids = waterFlowTowardsShortestPath(pos, liquid, liquid->currentPaths);
			newLiquids.insert(newLiquids.end(),
					extraLiquids.begin(),
					extraLiquids.end());
			liquid->currentPaths.clear();
			continue ;
		}

		for (const auto &dir : directions)
		{
			glm::ivec3 newPosition(pos + dir);
			if (visitedPositions.contains(newPosition)) continue;
			visitedPositions.insert(newPosition);

			BlockType neighbor = getBlockWorld(newPosition);
			if (neighbor == BlockType::AIR)
			{
				std::shared_ptr<s_liquid> newLiquidPtr = std::make_shared<s_liquid>();
				newLiquidPtr->currPropagation = liquid->currPropagation - 1;
				newLiquidPtr->liquidType = liquid->liquidType;
				newLiquidPtr->position = newPosition;
				newLiquidPtr->source = liquidsManager.liquids[liquid->position];
				newLiquids.push_back(newLiquidPtr);
			}
		}
	}
	liquidsManager.liquidsToUpdate.clear();

	for (auto liquidKey : liquidsToRemove)
	{
		setBlockWorld(liquidKey, std::nullopt, BlockType::AIR);
		liquidsManager.liquids.erase(liquidKey);
	}

	for (auto &liquidPtr : newLiquids)
	{
		liquidsManager.addNewLiquid(liquidPtr->position, liquidPtr);
		if (liquidPtr->currPropagation >= 0)
			setWaterWorld(liquidPtr->position, std::nullopt, liquidPtr->liquidType);
	}
}

void World::saveRegionsOnExit()
{
	if (!SAVES_ACTIVE) return;
    for (auto it = loadedRegions.begin(); it != loadedRegions.end();) {
        saveRegion(it->first, it->second);
        it = loadedRegions.erase(it);
    }
}

void World::unloadChunksInRegion(int RegionX, int RegionZ)
{
	for (int x = RegionX * REGION_SIZE; x < (RegionX + 1) * REGION_SIZE; x++) {
		for (int z = RegionZ * REGION_SIZE; z < (RegionZ + 1) * REGION_SIZE; z++) {
			auto it = chunks.find(Chunk::toKey(x, z));
			if (it != chunks.end()) {
				chunks.erase(it);
			}
		}
	}
}

void World::updateRegionStreaming(std::vector<CPlayerInfo> &players)
{
	std::unordered_set<ChunkPos> regionsToKeep;

	for (auto &player : players)
	{
		ChunkPos playerChunk = player.movement->getChunkPos();
		int chunkX = playerChunk.first;
		int chunkZ = playerChunk.second;

		int regionX = floorDiv(chunkX, REGION_SIZE);
		int regionZ = floorDiv(chunkZ, REGION_SIZE);

		for (int dx = -RADIUS_OF_REGIONS_TO_KEEP; dx <= RADIUS_OF_REGIONS_TO_KEEP; ++dx) {
			for (int dz = -RADIUS_OF_REGIONS_TO_KEEP; dz <= RADIUS_OF_REGIONS_TO_KEEP; ++dz) {
				ChunkPos neighbor(regionX + dx, regionZ + dz);
				regionsToKeep.insert(neighbor);

				if (!loadedRegions.count(neighbor)) {
					loadRegion(neighbor.first, neighbor.second);
					loadedRegions.insert(neighbor);
				}
			}
		}
	}

    // Unload? save regions that are not in the 3x3 grid
    for (auto it = loadedRegions.begin(); it != loadedRegions.end();) {
        if (!regionsToKeep.count(*it)) {

			//Save region if possible
			if (!outOfMemory && SAVES_ACTIVE) {
				try {
					saveRegion(it->first, it->second);
				} catch (std::exception &e) {
					std::cerr << "Error in updateRegionStreaming: " << e.what() << " - possibly out of memory." << std::endl;

					outOfMemory = true;
				}
			}
			else
			{
				unloadChunksInRegion(it->first, it->second);
			}

            it = loadedRegions.erase(it);
        } else {
            ++it;
        }
    }
}

void World::saveRegion(int regionX, int regionZ) {
	std::string filename = getRegionFilename(regionX, regionZ);

	// Write into memory buffer first
	std::ostringstream oss(std::ios::binary);

	// --- Write metadata ---
	RegionFileMetadata metadata;
	oss.write(reinterpret_cast<const char*>(&metadata), sizeof(metadata));

	// --- Reserve header space ---
	std::vector<ChunkEntry> header(REGION_SIZE * REGION_SIZE);
	oss.write(reinterpret_cast<const char*>(header.data()), header.size() * sizeof(ChunkEntry));

	// --- Write chunks ---
	for (int x = regionX * REGION_SIZE; x < (regionX + 1) * REGION_SIZE; x++) {
		for (int z = regionZ * REGION_SIZE; z < (regionZ + 1) * REGION_SIZE; z++) {
			auto it = chunks.find(Chunk::toKey(x, z));
			if (it == chunks.end()) continue;

			std::streampos currPos = oss.tellp();
			it->second->saveToStream(oss);
			std::streampos newPos = oss.tellp();

			ChunkEntry entry;
			entry.X = it->first.first;
			entry.Z = it->first.second;
			entry.offset = static_cast<std::uint32_t>(currPos);
			entry.size   = static_cast<std::uint32_t>(newPos - currPos);

			int localX = x - regionX * REGION_SIZE;
			int localZ = z - regionZ * REGION_SIZE;
			int idx = localZ * REGION_SIZE + localX;

			header[idx] = entry;

			chunks.erase(it);
		}
	}

	// --- Rewrite header in memory ---
	oss.seekp(sizeof(metadata));
	oss.write(reinterpret_cast<const char*>(header.data()), header.size() * sizeof(ChunkEntry));

	// === Compress entire buffer ===
	std::string rawData = oss.str();
	size_t maxCompressedSize = ZSTD_compressBound(rawData.size());
	std::vector<uint8_t> compressed(maxCompressedSize);

	size_t compressedSize = ZSTD_compress(compressed.data(), maxCompressedSize,
										rawData.data(), rawData.size(), /*level*/ 3);
	if (ZSTD_isError(compressedSize)) {
		throw std::runtime_error("ZSTD compression failed: " + std::string(ZSTD_getErrorName(compressedSize)));
	}
	compressed.resize(compressedSize);

	// === Write compressed file ===
	std::ofstream out(filename, std::ios::binary | std::ios::trunc);
	if (!out) throw std::runtime_error("Cannot open region file for writing: " + filename);

	out.write(reinterpret_cast<const char*>(compressed.data()), compressed.size());
}

void World::loadRegion(int regionX, int regionZ) {
	std::string filename = getRegionFilename(regionX, regionZ);
	std::ifstream in(filename, std::ios::binary);
	if (!in) return;

	// Read whole compressed file into memory
	std::vector<uint8_t> compressed((std::istreambuf_iterator<char>(in)), {});
	if (compressed.empty()) return;

	// Figure out decompressed size (if stored in frame)
	unsigned long long decompressedSize = ZSTD_getFrameContentSize(compressed.data(), compressed.size());
	if (decompressedSize == ZSTD_CONTENTSIZE_ERROR) {
		throw std::runtime_error("Not a valid ZSTD stream: " + filename);
	}
	if (decompressedSize == ZSTD_CONTENTSIZE_UNKNOWN) {
		throw std::runtime_error("Unknown decompressed size for: " + filename);
	}

	std::vector<uint8_t> decompressed(decompressedSize);

	size_t actualSize = ZSTD_decompress(decompressed.data(), decompressedSize,
										compressed.data(), compressed.size());
	if (ZSTD_isError(actualSize)) {
		throw std::runtime_error("ZSTD decompression failed: " + std::string(ZSTD_getErrorName(actualSize)));
	}

	// Now parse from memory (like a file stream)
	std::istringstream iss(std::string(reinterpret_cast<char*>(decompressed.data()), actualSize));

	// --- Read metadata ---
	RegionFileMetadata metadata;
	iss.read(reinterpret_cast<char*>(&metadata), sizeof(metadata));
	if (std::strncmp(metadata.magic, "RGN1", 4) != 0)
		throw std::runtime_error("Invalid region file magic in " + filename);

	// --- Read header ---
	std::vector<ChunkEntry> header(REGION_SIZE * REGION_SIZE);
	iss.read(reinterpret_cast<char*>(header.data()), header.size() * sizeof(ChunkEntry));

	// --- Load chunks ---
	for (const auto& entry : header) {
		if (entry.size == 0 || entry.offset == 0) continue;

		iss.seekg(entry.offset);
		auto chunk = std::make_shared<ChunkGeneration>(entry.X, entry.Z, terrainParams, false);
		chunk->loadFromStream(iss);
		linkNeighbors(entry.X, entry.Z, chunk);

		ChunkPos pos(entry.X, entry.Z);
		chunks[pos] = chunk;
	}
}

std::string World::getRegionFilename(int regionX, int regionZ) const {
	
    std::ostringstream ss;
    ss << regionDirName + "/r." << regionX << "." << regionZ << ".rg";
    return ss.str();
}

bool World::setBlockWorld(glm::ivec3 globalCoords, std::optional<glm::ivec3> faceNormal, BlockType type)
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

    std::shared_ptr<Chunk> currChunk = it->second;

    // When placing a block, if the target position contains vegetation, replace it
    if (type != BlockType::AIR) {
        BlockType targetBlock = currChunk->getBlock(x, y, z);
        if (isBlockVegetation(targetBlock)) {
            auto& vegList = currChunk->vegetation;
            vegList.erase(
                std::remove_if(vegList.begin(), vegList.end(),
                    [x, y, z](const Chunk::VegetationInstance& v) {
                        return v.x == x && v.y == y && v.z == z;
                    }),
                vegList.end()
            );
        }
    }

	updatedBlocks.push_back({targetCoords, type});

    // Handle vegetation: if breaking a block that has vegetation above, remove the vegetation
    if (type == BlockType::AIR && y + 1 < Chunk::HEIGHT) {
        BlockType blockAbove = currChunk->getBlock(x, y + 1, z);
        if (isBlockVegetation(blockAbove)) {
            // Remove vegetation from the chunk's vegetation list
            auto& vegList = currChunk->vegetation;
            vegList.erase(
                std::remove_if(vegList.begin(), vegList.end(),
                    [x, yAbove = y + 1, z](const Chunk::VegetationInstance& v) {
                        return v.x == x && v.y == yAbove && v.z == z;
                    }),
                vegList.end()
            );
            // Also set the block above to AIR
            currChunk->setBlock(x, y + 1, z, BlockType::AIR);
            updatedBlocks.push_back({targetCoords + glm::ivec3(0, 1, 0), BlockType::AIR});
        }
    }

    // If breaking vegetation directly, remove it from the vegetation list
    BlockType oldBlock = currChunk->getBlock(x, y, z);
    if (isBlockVegetation(oldBlock) && type == BlockType::AIR) {
        auto& vegList = currChunk->vegetation;
        vegList.erase(
            std::remove_if(vegList.begin(), vegList.end(),
                [x, y, z](const Chunk::VegetationInstance& v) {
                    return v.x == x && v.y == y && v.z == z;
                }),
            vegList.end()
        );
    }

    currChunk->setBlock(x, y, z, type);

	// update neat water blocks
	static const glm::ivec3 directions[6] = {
		{ 0, -1,  0}, // Down
		{ 0,  1,  0}, // Up
		{-1,  0,  0}, // Left
		{ 1,  0,  0}, // Right
		{ 0,  0,  1}, // Front
		{ 0,  0, -1}  // Back
	};

	if (type == BlockType::WATER)
	{
		auto liquidPtr = std::make_shared<s_liquid>();

		liquidPtr->liquidType = BlockType::WATER;
		liquidPtr->position = targetCoords;
		liquidPtr->source = liquidPtr;

		liquidsManager.addNewLiquid(targetCoords, liquidPtr);
	}
	else
	{
		// if water was there, removed it
		auto liquidIt = liquidsManager.liquids.find(targetCoords);
		if (liquidIt != liquidsManager.liquids.end())
			liquidsManager.liquids.erase(targetCoords);

		// if water is near update it
		for (auto &dir : directions)
		{
			auto it = liquidsManager.liquids.find(targetCoords + dir);
			if (it != liquidsManager.liquids.end())
				liquidsManager.liquidsToUpdate[it->first] = it->second;
			else if (it == liquidsManager.liquids.end() && getBlockWorld(targetCoords + dir) == BlockType::WATER) //create new water if we have updated a block next to a generated water block
			{
				auto liquidPtr = std::make_shared<s_liquid>();

				liquidPtr->liquidType = BlockType::WATER;
				liquidPtr->position = targetCoords + dir;
				liquidPtr->source = liquidPtr;

				liquidsManager.addNewLiquid(targetCoords + dir, liquidPtr);
			}
		}
	}

	return true;
}

void World::setWaterWorld(glm::ivec3 globalCoords, std::optional<glm::ivec3> faceNormal, BlockType type)
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

	updatedBlocks.push_back({targetCoords, type});
	currChunk->setBlock(x, y, z, type);
}

bool World::processPlayerMouseInputs(CPlayerInfo &player, const NetPlayerMouseInputs &pkt, int32_t serverTick)
{
	//Repetition. Not clean. And not performance friendly either.
	glm::ivec3 blockPos, faceNormal;
	getTargetedBlock(player.movement->getPosition(), player.movement->getCameraDir(), blockPos, faceNormal);
	BlockType dropped = getBlockWorld(blockPos);

	ItemType item = player.movement->inventory.getItemAtSlot(player.movement->inventory.activeHotbarSlot);

	if (pkt.mouseButtons & IN_RIGHT_CLICK && std::holds_alternative<BlockType>(item) && std::get<BlockType>(item) != BlockType::BEGIN) 
	{
		if (setTargettedBlock(player.movement->getPosition(), player.movement->getCameraDir(), std::get<BlockType>(item)))
		{
			player.movement->inventory.removeItemsFromSlot(player.movement->inventory.activeHotbarSlot, 1);
			return true;
		}
		return false;
	}
	if (pkt.mouseButtons & IN_LEFT_CLICK)
	{
		if (removeTargettedBlock(player.movement->getPosition(), player.movement->getCameraDir()) && player.movement->gamemode == GAMEMODES::SURVIVAL)
		{
			// random generator
			static std::mt19937 rng(std::random_device{}());
			std::uniform_real_distribution<float> angleDist(0.0f, 360.0f);
			std::uniform_real_distribution<float> offsetDist(-0.25f, 0.25f);

			// pick a random yaw angle (in degrees)
			float randomAngle = angleDist(rng);

			// convert to radians for glm
			float yawRad = glm::radians(randomAngle);

			// small position offset from the block center
			glm::vec3 positionOffset = glm::normalize(glm::vec3(std::cos(yawRad), 0.0f, std::sin(yawRad))) 
									* 0.15f; // radius offset

			// optional: add some slight random variation so they don’t stack perfectly
			positionOffset.x += offsetDist(rng);
			positionOffset.z += offsetDist(rng);

			// spawn the entity at block center + offset
			glm::vec3 spawnPos = glm::vec3(blockPos) + glm::vec3(0.5f) + positionOffset;

			itemEntities.push_back(std::make_shared<ItemEntity>(spawnPos, randomAngle, dropped, serverTick));
		}
	}
	return false;
}

void World::updateEntitiesPosition(const std::vector<CPlayerInfo> &players, int32_t serverTick)
{
	for (auto &entity : livingEntities)
		entity->calculateNewPosition(*this);

	for (auto entityIt = itemEntities.begin(); entityIt != itemEntities.end();)
	{
		//Checks for every prop if there's a player nearby that can pick it up. TODO: if this is too expensive do it every n ticks instead.
		if (entityIt->get()->getSpawnTick() + TPS * 1.5 < serverTick)
		{
			bool itemErased = false;
			for (auto &player : players)
			{
				if (player.movement.get()->gamemode != GAMEMODES::SURVIVAL)
					continue ;

				glm::vec3 diff = player.movement->getPosition() - entityIt->get()->getPosition();
				if (abs(diff.x) < 2 &&
					diff.y >= 0 && diff.y < 4 &&
					abs(diff.z) < 2)
				{
					int one = 1;
					int slotUsed = player.movement->inventory.insertItems(entityIt->get()->getItemType(), one);
					if (slotUsed == INVALID_SLOT) continue ;

					NetEntityMove pkt;
					pkt.eEntityType = entityIt->get()->getEntityType();
					pkt.entityID = entityIt->get()->getID();
					pkt.type = -1;

					pkt.positionX = player.movement->getPosition().x;
					pkt.positionY = player.movement->getPosition().y;
					pkt.positionZ = player.movement->getPosition().z;
					pkt.yaw = entityIt->get()->yaw;

					deletedEntitiesPkts.push_back(pkt);

					NetInventory pickedUpItem;
					pickedUpItem.type = entityIt->get()->getItemID();
					pickedUpItem.amount = player.movement->inventory.getSlot(slotUsed).second;
					pickedUpItem.slot = slotUsed;
					pickedUpItems.push_back({player.addr, pickedUpItem});

					entityIt = itemEntities.erase(entityIt);
					itemErased = true;

					break ;
				}
			}
			if (itemErased) continue ;
		}

		entityIt->get()->calculateNewPosition(*this);
		entityIt++;
	}
}