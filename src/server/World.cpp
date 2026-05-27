//
// Created by lucas on 7/1/25.
//

#include "World.hpp"
#include "Mobs/Creeper.hpp"

static std::mt19937 spawnRng(std::random_device{}());

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
void World::dumpHeightmap(const TerrainGenerationParams& params, int centerChunkX, int centerChunkZ, int chunksX, int chunksZ, int downsample, int image) const {
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

	if (outW <= 0 || outH <= 0 || static_cast<size_t>(outW) * static_cast<size_t>(outH) > MAX_DUMP_PIXELS) {
        std::cerr << "[World] dumpHeightmap: " << outW << "x" << outH << " exceeds limit. Aborting.\n";
        return;
    }

    const size_t npixels = static_cast<size_t>(outW) * outH;
	std::vector<float> img, imgCont, imgEro, imgPV, imgHumidity, imgTemperature;
	std::vector<float> imgRiverNoise, imgRiverMask, imgLakeNoise, imgLakeMask;

	if (image == 0) {
		img.resize(npixels);
	} else if (image == 1) {
		imgCont.resize(npixels); imgEro.resize(npixels); imgPV.resize(npixels);
		imgHumidity.resize(npixels); imgTemperature.resize(npixels);
		imgRiverNoise.resize(npixels); imgRiverMask.resize(npixels);
		imgLakeNoise.resize(npixels); imgLakeMask.resize(npixels);
	} else if (image == 2) {
		imgRiverNoise.resize(npixels); imgRiverMask.resize(npixels);
		imgLakeNoise.resize(npixels); imgLakeMask.resize(npixels);
	}

    // Precompute erosion spline range once - used in every pixel
    float eroMin = std::numeric_limits<float>::infinity();
    float eroMax = -std::numeric_limits<float>::infinity();
    for (const auto& sp : erosionSpline) {
        eroMin = glm::min(eroMin, sp.second);
        eroMax = glm::max(eroMax, sp.second);
    }

    for (int oz = 0, wz = 0; wz < outH; ++wz, oz += downsample) {
        for (int ox = 0, wx = 0; wx < outW; ++wx, ox += downsample) {
            const auto worldX = static_cast<float>(startX + ox);
            const auto worldZ = static_cast<float>(startZ + oz);

        	const float continentalness = ChunkGeneration::getContinentalness(params, worldX, worldZ);
        	const float erosion = ChunkGeneration::getErosion(params, worldX, worldZ);
        	const float pv = ChunkGeneration::getPV(params, worldX, worldZ);
            const float baseHeight = ChunkGeneration::surfaceNoiseTransformation(continentalness, 1);
            const float erosionSplineValue = ChunkGeneration::surfaceNoiseTransformation(erosion, 2);
            const float pvSplineValue = ChunkGeneration::surfaceNoiseTransformation(pv, 3);

            float erosionNorm = 0.0f;
            if (eroMax > eroMin)
                erosionNorm = glm::clamp((erosionSplineValue - eroMin) / (eroMax - eroMin), 0.0f, 1.0f);
            erosionNorm = 1.0f - erosionNorm;

            const float inlandMask = glm::smoothstep(-0.19f, 3.8f, continentalness);
            constexpr float minErosionStrength = 2.0f;
            constexpr float maxErosionStrength = 140.0f;
            const float erosionStrength = glm::mix(minErosionStrength, maxErosionStrength, inlandMask);
            const float erosionDelta = erosionNorm * erosionStrength;
            const float pvFactor = pvSplineValue * (1.0f - erosionNorm);
            const float preHydroHeight = baseHeight - erosionDelta + pvFactor;

            if (image == 0) {

                const int surfaceY = ChunkGeneration::computeTerrainHeight(params, worldX, worldZ);

                img[wx + wz * outW] = surfaceY;
                // imgCont[wx + wz * outW] = baseHeight;
                // imgEro[wx + wz * outW] = erosionDelta;
                // imgPV[wx + wz * outW] = pvFactor;
            } else if (image == 1) {
                imgCont[wx + wz * outW] = continentalness;
                imgEro[wx + wz * outW] = erosion;
                imgPV[wx + wz * outW] = pv;
            	imgHumidity[wx + wz * outW] = ChunkGeneration::getHumidity(params, worldX, worldZ);
            	imgTemperature[wx + wz * outW] = ChunkGeneration::getTemperature(params, worldX, worldZ);
                imgRiverNoise[wx + wz * outW] = ChunkGeneration::getRiverNoise(params, worldX, worldZ);
                const float riverMask = ChunkGeneration::getRiverMask(params, worldX, worldZ, continentalness, preHydroHeight, pv);
                imgRiverMask[wx + wz * outW] = std::pow(glm::clamp(riverMask, 0.0f, 1.0f), 0.45f);
                imgLakeNoise[wx + wz * outW] = ChunkGeneration::getLakeNoise(params, worldX, worldZ);
                const float lakeMask = ChunkGeneration::getLakeMask(params, worldX, worldZ, continentalness, preHydroHeight, pv);
                imgLakeMask[wx + wz * outW] = std::pow(glm::clamp(lakeMask, 0.0f, 1.0f), 0.35f);
            } else if (image == 2) {
                imgRiverNoise[wx + wz * outW] = ChunkGeneration::getRiverNoise(params, worldX, worldZ);
                const float riverMask = ChunkGeneration::getRiverMask(params, worldX, worldZ, continentalness, preHydroHeight, pv);
                imgRiverMask[wx + wz * outW] = std::pow(glm::clamp(riverMask, 0.0f, 1.0f), 0.45f);
                imgLakeNoise[wx + wz * outW] = ChunkGeneration::getLakeNoise(params, worldX, worldZ);
                const float lakeMask = ChunkGeneration::getLakeMask(params, worldX, worldZ, continentalness, preHydroHeight, pv);
                imgLakeMask[wx + wz * outW] = std::pow(glm::clamp(lakeMask, 0.0f, 1.0f), 0.35f);
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
        saveHeightmapPPM("riverNoise.ppm", imgRiverNoise, outW, outH);
        saveHeightmapPPM("riverMask.ppm", imgRiverMask, outW, outH);
        saveHeightmapPPM("lakeNoise.ppm", imgLakeNoise, outW, outH);
        saveHeightmapPPM("lakeMask.ppm", imgLakeMask, outW, outH);
    } else if (image == 2) {
        saveHeightmapPPM("riverNoise.ppm", imgRiverNoise, outW, outH);
        saveHeightmapPPM("riverMask.ppm", imgRiverMask, outW, outH);
        saveHeightmapPPM("lakeNoise.ppm", imgLakeNoise, outW, outH);
        saveHeightmapPPM("lakeMask.ppm", imgLakeMask, outW, outH);
    }
}

// Dump a color-coded biome map as a PPM. Each column maps to a pixel.
void World::dumpBiomeMap(const TerrainGenerationParams& params, int centerChunkX, int centerChunkZ, int chunksX, int chunksZ, int downsample) {
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
    if (imgW <= 0 || imgH <= 0 ||
		static_cast<size_t>(imgW) * static_cast<size_t>(imgH) > MAX_DUMP_PIXELS) {
		std::cerr << "[World] dumpHeightmap: " << imgW << "x" << imgH << " exceeds limit. Aborting.\n";
		return;
	}

	

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
            const int height = ChunkGeneration::computeTerrainHeight(params, wx, wz);
            const BiomeType biome = ChunkGeneration::computeBiome(params, wx, wz, height);

            glm::u8vec3 color;
            switch (biome) {
                case BiomeType::PLAINS:  color = { 4, 130, 67 }; break;
                case BiomeType::DESERT:  color = { 255, 255, 0 }; break;
                case BiomeType::DARK_FOREST:  color = { 19, 77, 19 }; break;
                case BiomeType::TUNDRA:  color = { 255, 255, 255 }; break;
                case BiomeType::SWAMP:   color = { 100, 70, 30 }; break;
                case BiomeType::OCEAN:   color = { 40, 60, 160 }; break;
            	case BiomeType::MOUNTAIN: color = { 100, 100, 100 }; break;
				case BiomeType::BIRCH_FOREST: color = { 177, 240, 177 }; break;
				case BiomeType::JUNGLE: color = { 0, 255, 0 }; break;
				case BiomeType::SAVANNA: color = { 175, 191, 0 }; break;
				case BiomeType::MESA: color = { 255, 0, 0 }; break;
				case BiomeType::ICE_PLAINS: color = { 87, 252, 255 }; break;
				case BiomeType::VOLCANIC: color = { 0, 0, 0 }; break;
				case BiomeType::RED_DESERT: color = { 255, 115, 0 }; break;
				case BiomeType::NETHER: color = { 183, 52, 235 }; break;
				case BiomeType::MUSHROOM_ISLAND: color = { 255, 183, 168 }; break;
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
		{
			// Already-generated chunk: queue directly for THIS player. Going
			// through the shared world->rdyChunks would re-send the chunk to
			// every other player whose PlayerKnownChunks already contains it,
			// causing visible chunk-blink for the others every time another
			// player requests an already-cached chunk.
			player.rdyChunks.push_back(bestChunk);
		}
		else
		{
			plannedChunks.insert(bestChunk);
			const TerrainGenerationParams paramsCopy = terrainParams;
			chunkJobs[bestChunk] = std::async(std::launch::async, [bestChunk, paramsCopy]() {
				return std::make_shared<ChunkGeneration>(bestChunk.first, bestChunk.second, paramsCopy);
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
	const int baseChunkX = static_cast<int>(std::floor(player.movement->getPosition().x / Chunk::WIDTH));
	const int baseChunkZ = static_cast<int>(std::floor(player.movement->getPosition().z / Chunk::DEPTH));
	const int r2 = player.movement->loadRadius * player.movement->loadRadius;
	auto& known = PlayerKnownChunks[player.id];

	for (const auto& chunkPos : rdyChunks)
	{
		if (known.find(chunkPos) != known.end())
		{
			player.rdyChunks.push_back(chunkPos);
			continue;
		}

		// Recover chunks orphaned by a radius shrink that happened while they were being generated:
		// they finished after we removed them from PlayerKnownChunks, so findNextChunk never re-requested them.
		const int dx = chunkPos.first - baseChunkX;
		const int dz = chunkPos.second - baseChunkZ;
		if (dx * dx + dz * dz <= r2)
		{
			known.insert(chunkPos);
			player.rdyChunks.push_back(chunkPos);
		}
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

				if (type == BlockType::AIR || type == BlockType::WATER || isBlockVegetation(type))
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

		BlockType block = getBlockWorld(position);
		if (block != BlockType::AIR && !isBlockVegetation(block)) continue ;

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
			if (neighbor == BlockType::AIR || isBlockVegetation(neighbor))
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
				if (SAVES_ACTIVE)
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
    int x, y, z;
    auto currChunk = resolveTarget(globalCoords, faceNormal, x, y, z);
    if (!currChunk)
        return false;

    glm::ivec3 targetCoords = globalCoords + faceNormal.value_or(glm::ivec3{0});
	updatedBlocks.push_back({targetCoords, type});

    // Place/break the block, cascading to clear any land vegetation above when breaking.
    // Land vegetation is tracked only in the block grid; the vegetation list holds sea veg only.
    bool clearedVegAbove = currChunk->setBlockCascade(x, y, z, type);
    if (clearedVegAbove)
        updatedBlocks.push_back({targetCoords + glm::ivec3(0, 1, 0), BlockType::AIR});

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

void World::processPlayerMouseInputs(CPlayerInfo &player, const NetPlayerMouseInputs &pkt, int32_t clientTick, std::vector<PacketPtr> &pktsToSend)
{
	LivingEntity* livingEntity = nullptr;
	glm::ivec3 blockPos, faceNormal;

	int activeSlot = player.movement->inventory->activeHotbarSlot;
	ItemType item = player.movement->inventory->getItemAtSlot(activeSlot);
	bool emptyBucketInHand = std::holds_alternative<MiscType>(item) && std::get<MiscType>(item) == MiscType::EMPTY_BUCKET && pkt.mouseButtons & IN_RIGHT_CLICK;

	TargetType target = getTarget(*player.movement, blockPos, faceNormal, livingEntity, !emptyBucketInHand);

	//place/remove Water with bucket
	if (std::holds_alternative<MiscType>(item))
	{
		MiscType m = std::get<MiscType>(item);

		if (pkt.mouseButtons & IN_RIGHT_CLICK)
		{
			if (m == MiscType::WATER_BUCKET)
			{
				if (target == TargetType::Block &&
					setBlockWorld(blockPos, faceNormal, BlockType::WATER))
				{
					player.movement->inventory->removeItemsFromSlot(activeSlot, 1);
					int one = 1;
					int slot = player.movement->inventory->insertItems(MiscType::EMPTY_BUCKET, one);
					pktsToSend.push_back(player.movement->inventory->createNetInventoryPkt(activeSlot));
					if (slot != activeSlot)
						pktsToSend.push_back(player.movement->inventory->createNetInventoryPkt(slot));
					return ;
				}
			}
			else if (m == MiscType::EMPTY_BUCKET)
			{
				if (target == TargetType::Block &&
					getBlockWorld(blockPos) == BlockType::WATER)
				{
					if (setBlockWorld(blockPos, std::nullopt, BlockType::AIR))
					{
						player.movement->inventory->removeItemsFromSlot(activeSlot, 1);
						int one = 1;
						int slot = player.movement->inventory->insertItems(MiscType::WATER_BUCKET, one);
						pktsToSend.push_back(player.movement->inventory->createNetInventoryPkt(activeSlot));
						if (slot != activeSlot)
							pktsToSend.push_back(player.movement->inventory->createNetInventoryPkt(slot));
						return ;
					}
				}
			}
		}
	}

	//set spawn
	if (pkt.mouseButtons & IN_RIGHT_CLICK && target == TargetType::Block && getBlockWorld(blockPos) == BlockType::BEACON)
	{
		player.movement->hasBeaconSet = true;
		player.movement->beaconPos = blockPos;
		player.targetedMessages.push_back("[server] Spawn point set!");
		return ;
	}

	// Torch placement: orientation is derived from the clicked face. The held
	// item is always TORCH_FLOOR; placing on a side wall promotes it to the
	// matching wall variant. Torches can't hang from a ceiling.
	if (pkt.mouseButtons & IN_RIGHT_CLICK && std::holds_alternative<BlockType>(item)
		&& isTorch(std::get<BlockType>(item)))
	{
		if (target != TargetType::Block)
			return ;

		BlockType torchVariant;
		if (faceNormal == glm::ivec3(0, 1, 0))
			torchVariant = BlockType::TORCH_FLOOR;
		else if (faceNormal == glm::ivec3(1, 0, 0))
			torchVariant = BlockType::TORCH_WALL_EAST;
		else if (faceNormal == glm::ivec3(-1, 0, 0))
			torchVariant = BlockType::TORCH_WALL_WEST;
		else if (faceNormal == glm::ivec3(0, 0, 1))
			torchVariant = BlockType::TORCH_WALL_NORTH;
		else if (faceNormal == glm::ivec3(0, 0, -1))
			torchVariant = BlockType::TORCH_WALL_SOUTH;
		else
			return ; // underside / unsupported face

		// The clicked block must be a solid support (not air / another
		// torch), and the cell the torch would occupy must be empty so a
		// new torch never overwrites an existing one sharing that cell.
		if (!isBlockSolid(getBlockWorld(blockPos)))
			return ;
		if (getBlockWorld(blockPos + faceNormal) != BlockType::AIR)
			return ;

		// No entity-collision check: a torch has no real hitbox, so you can
		// place one in the cell you're standing in (unlike a full block).
		if (setBlockWorld(blockPos, faceNormal, torchVariant))
		{
			player.movement->inventory->removeItemsFromSlot(player.movement->inventory->activeHotbarSlot, 1);
			pktsToSend.push_back(player.movement->inventory->createNetInventoryPkt(activeSlot));
			return ;
		}
		return ;
	}

	if (pkt.mouseButtons & IN_RIGHT_CLICK && std::holds_alternative<BlockType>(item) && std::get<BlockType>(item) != BlockType::BEGIN)
	{
		if (target == TargetType::Block)
		{
			//set block
			for (const auto &entity : livingEntities)
				if (entity->entityCollidesWithBlock(blockPos + faceNormal)) return ; //only checks collision with living entities
			if (setBlockWorld(blockPos, faceNormal, std::get<BlockType>(item)))
			{
				player.movement->inventory->removeItemsFromSlot(activeSlot, 1);
				pktsToSend.push_back(player.movement->inventory->createNetInventoryPkt(activeSlot));
				return ;
			}
		}
	}
	else if (pkt.mouseButtons & IN_LEFT_CLICK)
	{
		if (target == TargetType::Block && player.movement->gamemode == GAMEMODES::SURVIVAL)
		{
			BlockType dropped = getBlockWorld(blockPos);

			if (dropped == BlockType::BEDROCK) return ;

			// Torches always drop as the inventory (floor) form so they
			// stack regardless of which wall variant was mined.
			if (isTorch(dropped)) dropped = BlockType::TORCH_FLOOR;

			// Ore blocks drop their raw resource item rather than the block.
			ItemType droppedItem = dropped;
			switch (dropped)
			{
				case BlockType::DIAMOND:  droppedItem = MiscType::DIAMOND;        break;
				case BlockType::URANIUM:  droppedItem = MiscType::URANIUM_INGOT;  break;
				case BlockType::COAL:     droppedItem = MiscType::COAL;           break;
				default: break;
			}

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

			itemEntities.push_back(std::make_shared<ItemEntity>(spawnPos, randomAngle, droppedItem, clientTick));
		}
		else if (target == TargetType::LivingEntity && livingEntity)
		{
			player.movement->attack(*livingEntity);
		}

		if (target == TargetType::Block)
			if (setBlockWorld(blockPos, std::nullopt, BlockType::AIR))
			{
				breakDependentTorches(blockPos, clientTick,
					player.movement->gamemode == GAMEMODES::SURVIVAL);
				return ;
			}
	}
	return ;
}

void World::breakDependentTorches(const glm::ivec3& removedBlock, int32_t clientTick, bool dropItems)
{
	// outDir for each wall variant: the direction the torch points away from
	// the wall. A wall torch in cell C is supported by the block at C-outDir,
	// so it depends on `removedBlock` when its cell == removedBlock + outDir.
	auto wallOutDir = [](BlockType t) -> glm::ivec3 {
		switch (t) {
			case BlockType::TORCH_WALL_EAST:  return { 1, 0,  0};
			case BlockType::TORCH_WALL_WEST:  return {-1, 0,  0};
			case BlockType::TORCH_WALL_NORTH: return { 0, 0,  1};
			case BlockType::TORCH_WALL_SOUTH: return { 0, 0, -1};
			default:                          return { 0, 0,  0};
		}
	};

	std::vector<glm::ivec3> toBreak;

	// Floor torch sitting on top of the removed block.
	const glm::ivec3 above = removedBlock + glm::ivec3(0, 1, 0);
	if (getBlockWorld(above) == BlockType::TORCH_FLOOR)
		toBreak.push_back(above);

	// Wall torch mounted on the removed block: it sits in the cell offset by
	// its outDir from the wall, so check the 4 horizontal neighbours.
	static const glm::ivec3 horiz[4] = {
		{ 1, 0, 0}, {-1, 0, 0}, { 0, 0, 1}, { 0, 0, -1}
	};
	for (const glm::ivec3& d : horiz) {
		const glm::ivec3 cell = removedBlock + d;
		BlockType b = getBlockWorld(cell);
		if (isTorch(b) && wallOutDir(b) == d)
			toBreak.push_back(cell);
	}

	for (const glm::ivec3& torchPos : toBreak) {
		if (!setBlockWorld(torchPos, std::nullopt, BlockType::AIR))
			continue;
		if (!dropItems)
			continue;
		static std::mt19937 rng(std::random_device{}());
		std::uniform_real_distribution<float> angleDist(0.0f, 360.0f);
		float randomAngle = angleDist(rng);
		glm::vec3 spawnPos = glm::vec3(torchPos) + glm::vec3(0.5f);
		itemEntities.push_back(std::make_shared<ItemEntity>(
			spawnPos, randomAngle, BlockType::TORCH_FLOOR, clientTick));
	}
}

void World::updateEntitiesPosition(const std::vector<CPlayerInfo> &players, int32_t clientTick)
{
	// Snapshot pending creeper explosions: we apply them after the tickAI/move pass so we
	// don't mutate livingEntities health mid-iteration in unexpected ways.
	std::vector<Creeper *> exploding;
	for (auto &entity : livingEntities)
	{
		// Freeze AI while the fall-over death animation plays out server-side.
		if (entity->pendingDeathRemovalTicks > 0 || entity->health <= 0)
			continue;
		entity->tickAI(*this, livingEntities, clientTick);
		entity->calculateNewPosition(*this);
		if (auto *creeper = dynamic_cast<Creeper *>(entity.get())) {
			if (creeper->wantsExplode) {
				creeper->wantsExplode = false;
				exploding.push_back(creeper);
			}
		}
	}
	for (Creeper *creeper : exploding)
		explodeAt(creeper->getPosition(), creeper->explodeRadius, creeper->explodeDamage,
		          creeper->explodeKnockH, creeper->explodeKnockV, creeper);

	for (auto entityIt = itemEntities.begin(); entityIt != itemEntities.end();)
	{
		//Checks for every prop if there's a player nearby that can pick it up. TODO: if this is too expensive do it every n ticks instead.
		if (entityIt->get()->getSpawnTick() + TPS * 0.5 < clientTick)
		{
			bool itemErased = false;
			for (auto &player : players)
			{
				if (player.movement.get()->gamemode != GAMEMODES::SURVIVAL)
					continue ;

				glm::vec3 diff = entityIt->get()->getPosition() - player.movement->getPosition();
				if (std::abs(diff.x) < 2 &&
					diff.y >= -1 && diff.y < 2 &&
					std::abs(diff.z) < 2)
				{
					int one = 1;
					int slotUsed = player.movement->inventory->insertItems(entityIt->get()->getItemType(), one);
					if (slotUsed == INVALID_SLOT) continue ;

					NetEntityMove pkt;
					pkt.eEntityType = entityIt->get()->getEntityType();
					pkt.entityID = entityIt->get()->getID();
					pkt.type = -1;

					pkt.positionX = player.movement->getPosition().x;
					pkt.positionY = player.movement->getPosition().y + player.movement->getEyesHeight();
					pkt.positionZ = player.movement->getPosition().z;
					pkt.yaw = entityIt->get()->yaw;

					deletedEntitiesPkts.push_back(pkt);

					NetInventory pickedUpItem;
					pickedUpItem.inventoryTypeID = static_cast<uint8_t>(InventoryType::PLAYER);
					pickedUpItem.type = entityIt->get()->getItemID();
					pickedUpItem.amount = player.movement->inventory->getSlot(slotUsed).second;
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

void World::advanceSkyTime() {
	// Advance sky time
	constexpr float tickDt        = 1.0f / TPS;
	constexpr float pauseDuration = 20.0f;
	constexpr float stepDuration  = 2.0f;

	if (!skyTimeState.skyTimePaused) {
		if (skyTimeState.skyMode == 1) {
			// Smooth: continuous linear advancement
			skyTimeState.skyTimeOffset += skyTimeState.skyTimeSpeed * tickDt;
		} else {
			// Skyrim: hold then step
			if (!skyTimeState.sunStepping) {
				skyTimeState.sunPauseTimer += tickDt;
				if (skyTimeState.sunPauseTimer >= pauseDuration) {
					skyTimeState.sunPauseTimer = 0.0f;
					skyTimeState.sunStepping   = true;
					skyTimeState.sunStepTimer  = 0.0f;
				}
			} else {
				skyTimeState.sunStepTimer += tickDt;
				float t_step  = std::min(skyTimeState.sunStepTimer / stepDuration, 1.0f);
				float smoothT = t_step * t_step * (3.0f - 2.0f * t_step);
				float totalStepOffset = (pauseDuration + stepDuration) * skyTimeState.skyTimeSpeed;
				if (skyTimeState.sunStepTimer <= tickDt)
					skyTimeState.sunPauseTimer = skyTimeState.skyTimeOffset;  // first tick: store stepBase
				skyTimeState.skyTimeOffset = skyTimeState.sunPauseTimer + smoothT * totalStepOffset;
				if (t_step >= 1.0f) {
					skyTimeState.sunStepping   = false;
					skyTimeState.sunPauseTimer = 0.0f;
					skyTimeState.sunStepTimer  = 0.0f;
				}
			}
		}
	}
}

void World::setSkyTime(const SkyTimeState &newState) {
    skyTimeState.skyTimeOffset = newState.skyTimeOffset;
    skyTimeState.sunYawDeg     = newState.sunYawDeg;
    skyTimeState.skyTimePaused = newState.skyTimePaused;
    skyTimeState.skyMode       = newState.skyMode;
    skyTimeState.skyTimeSpeed  = newState.skyTimeSpeed;

    // reset step state after external set
    skyTimeState.sunStepping   = false;
    skyTimeState.sunPauseTimer = 0.0f;
    skyTimeState.sunStepTimer  = 0.0f;
}

void World::explodeAt(const glm::vec3 &center, float radius, float maxDamage,
                      float knockH, float knockV, LivingEntity *source)
{
    const float r2 = radius * radius;
    const int r = static_cast<int>(std::ceil(radius));

    const int cx = static_cast<int>(std::floor(center.x));
    const int cy = static_cast<int>(std::floor(center.y));
    const int cz = static_cast<int>(std::floor(center.z));

    // Carve a rough sphere of blocks to air. setBlockWorld pushes to updatedBlocks,
    // which the existing block-sync pipeline streams to every client.
    for (int dx = -r; dx <= r; ++dx) {
        for (int dy = -r; dy <= r; ++dy) {
            for (int dz = -r; dz <= r; ++dz) {
                const float d2 = float(dx*dx + dy*dy + dz*dz);
                if (d2 > r2) continue;
                const glm::ivec3 bpos{cx + dx, cy + dy, cz + dz};
                const BlockType b = getBlockWorld(bpos);
                if (b == BlockType::AIR || b == BlockType::BEDROCK || b == BlockType::END)
                    continue;
                setBlockWorld(bpos, std::nullopt, BlockType::AIR);
            }
        }
    }

    // Distance-falloff damage + knockback for every living entity inside the sphere.
    for (auto &entity : livingEntities) {
        if (!entity) continue;
        glm::vec3 diff = entity->getPosition() - center;
        float dist = glm::length(diff);
        if (dist > radius) continue;
        float falloff = 1.0f - (dist / radius);
        if (falloff <= 0.0f) continue;

        entity->health = std::max(0.0f, entity->health - maxDamage * falloff);
        if (entity.get() == source)
            entity->diedByExplosion = true;

        glm::vec3 knockDir = dist > EPS ? diff / dist : glm::vec3(0, 1, 0);
        entity->applyImpulse(knockDir * (falloff * knockH) + glm::vec3(0.0f, knockV * falloff, 0.0f));
    }
}

void World::trySpawnNightMobs(const std::vector<CPlayerInfo> &players)
{
	if (players.empty())
		return;

	// Night-time check: sun elevation is cos(skyTimeOffset * 0.1) (see Lighting.cpp).
	// Negative elevation means the sun is below the horizon - i.e. night.
	// Using this formulation avoids wrap-around issues as skyTimeOffset accumulates.
	// Surface spawning is gated on night; cave spawning runs regardless so mining
	// stays hazardous during the day too.
	const float skyT = getSkyTimeState().skyTimeOffset;
	const bool isNight = std::cos(skyT * 0.1f) < 0.0f;

	constexpr int MAX_ZOMBIES_PER_PLAYER  = 10;
	constexpr int MAX_CREEPERS_PER_PLAYER = 50;
	constexpr int MIN_SPAWN_DIST = 40;
	constexpr int MAX_SPAWN_DIST = 80;
	constexpr int SCAN_TOP_Y = 200;
	constexpr int SCAN_BOTTOM_Y = 4;

	// Cave spawning parameters - closer than surface spawns so mobs actually
	// show up near a mining player, and biased to Y bands around the player.
	constexpr int CAVE_MIN_DIST = 16;
	constexpr int CAVE_MAX_DIST = 48;
	// Floor must sit at least this many blocks below the column's surface to
	// count as a cave (rejects overhangs and shallow dirt cover).
	constexpr int CAVE_MIN_DEPTH = 8;
	// Vertical search band around the player
	constexpr int CAVE_Y_BELOW = 24;
	constexpr int CAVE_Y_ABOVE = 4;

	// Count current hostile mobs.
	int zombieCount  = 0;
	int creeperCount = 0;
	for (auto &e : livingEntities) {
		if (!e) continue;
		if (e->getLivingEntityType() == ZOMBIE)  zombieCount++;
		if (e->getLivingEntityType() == CREEPER) creeperCount++;
	}

	auto findGroundSpawn = [&](const glm::vec3 &ppos, glm::vec3 &out) -> bool {
		float angle = glm::radians(static_cast<float>(std::uniform_int_distribution<int>(0, 359)(spawnRng)));
		int dist = std::uniform_int_distribution<int>(MIN_SPAWN_DIST, MAX_SPAWN_DIST - 1)(spawnRng);
		int sx = static_cast<int>(std::floor(ppos.x + std::cos(angle) * dist));
		int sz = static_cast<int>(std::floor(ppos.z + std::sin(angle) * dist));

		auto isAir = [&](int y) {
			BlockType b = getBlockWorld({sx, y, sz});
			return b == BlockType::AIR;
		};
		auto isSpawnableGround = [&](int y) {
			BlockType b = getBlockWorld({sx, y, sz});
			return b != BlockType::END && isBlockSolid(b);
		};

		int groundY = -1;
		bool airAbove1 = isAir(SCAN_TOP_Y + 1);
		bool airAbove2 = isAir(SCAN_TOP_Y);
		for (int y = SCAN_TOP_Y; y >= SCAN_BOTTOM_Y; --y) {
			if (isSpawnableGround(y) && airAbove1 && airAbove2) {
				groundY = y;
				break;
			}
			airAbove2 = airAbove1;
			airAbove1 = isAir(y);
		}
		if (groundY < 0) return false;

		glm::vec3 spawnPos(sx + 0.5f, static_cast<float>(groundY + 1), sz + 0.5f);
		glm::vec3 d = spawnPos - ppos;
		if (d.x * d.x + d.z * d.z < float(MIN_SPAWN_DIST * MIN_SPAWN_DIST) * 0.25f) return false;
		out = spawnPos;
		return true;
	};

	// Picks a spawn position inside a cave near the player. Runs day or night.
	// Validates that the air pocket is genuinely below the surface (not just an overhang) before accepting.
	auto findCaveSpawn = [&](const glm::vec3 &ppos, glm::vec3 &out) -> bool {
		float angle = glm::radians(static_cast<float>(std::uniform_int_distribution<int>(0, 359)(spawnRng)));
		int dist = std::uniform_int_distribution<int>(CAVE_MIN_DIST, CAVE_MAX_DIST - 1)(spawnRng);
		int sx = static_cast<int>(std::floor(ppos.x + std::cos(angle) * dist));
		int sz = static_cast<int>(std::floor(ppos.z + std::sin(angle) * dist));

		auto block = [&](int y) { return getBlockWorld({sx, y, sz}); };
		auto isAir = [&](int y) { return block(y) == BlockType::AIR; };
		auto isCaveFloor = [&](int y) {
			BlockType b = block(y);
			// Bedrock floors would dump mobs at the world bottom; END is a non-spawnable marker.
			return b != BlockType::END && b != BlockType::BEDROCK && isBlockSolid(b);
		};

		// Locate the column's surface (topmost solid block). If the chunk is
		// unloaded the whole column reads as air and we bail.
		int surfaceY = -1;
		for (int y = SCAN_TOP_Y; y >= SCAN_BOTTOM_Y; --y) {
			BlockType b = block(y);
			if (b != BlockType::AIR && isBlockSolid(b)) { surfaceY = y; break; }
		}
		if (surfaceY < 0) return false;

		const int pY = static_cast<int>(std::floor(ppos.y));
		int yHi = std::min(surfaceY - CAVE_MIN_DEPTH, pY + CAVE_Y_ABOVE);
		int yLo = std::max(SCAN_BOTTOM_Y, pY - CAVE_Y_BELOW);
		if (yHi - yLo < 2) return false;

		// Scan downward from a random Y in the band looking for an air pocket
		// (2+ air blocks) sitting on a solid floor - somewhere a mob can stand.
		int startY = std::uniform_int_distribution<int>(yLo, yHi)(spawnRng);
		bool airAbove1 = isAir(startY + 2);
		bool airAbove2 = isAir(startY + 1);
		for (int y = startY; y >= yLo; --y) {
			if (isCaveFloor(y) && airAbove1 && airAbove2 && (surfaceY - y) >= CAVE_MIN_DEPTH) {
				glm::vec3 spawnPos(sx + 0.5f, static_cast<float>(y + 1), sz + 0.5f);
				glm::vec3 d = spawnPos - ppos;
				// Reject candidates right on top of the player (3D distance).
				if (d.x * d.x + d.y * d.y + d.z * d.z < 64.0f) return false;
				out = spawnPos;
				return true;
			}
			airAbove2 = airAbove1;
			airAbove1 = isAir(y);
		}
		return false;
	};

	for (auto &player : players) {
		if (!player.movement) continue;
		const glm::vec3 ppos = player.movement->getPosition();
		const int playerCap = static_cast<int>(players.size());

		// Zombies: try surface (night only) then a cave spawn, so an underground
		// player still gets one nearby spawn attempt per cycle.
		if (zombieCount < MAX_ZOMBIES_PER_PLAYER * playerCap) {
			bool spawned = false;
			if (isNight) {
				for (int attempt = 0; attempt < 5 && !spawned; ++attempt) {
					glm::vec3 spawnPos;
					if (!findGroundSpawn(ppos, spawnPos)) continue;
					livingEntities.push_back(std::make_shared<Zombie>(spawnPos));
					zombieCount++;
					spawned = true;
				}
			}
			if (!spawned && zombieCount < MAX_ZOMBIES_PER_PLAYER * playerCap) {
				for (int attempt = 0; attempt < 5; ++attempt) {
					glm::vec3 spawnPos;
					if (!findCaveSpawn(ppos, spawnPos)) continue;
					livingEntities.push_back(std::make_shared<Zombie>(spawnPos));
					zombieCount++;
					break;
				}
			}
		}

		// Creepers: higher cap AND a probability gate, only ~25% of cycles are
		// allowed to spawn a creeper at all, so they stay rarer than zombies.
		if (creeperCount < MAX_CREEPERS_PER_PLAYER * playerCap &&
			std::uniform_int_distribution<int>(0, 3)(spawnRng) == 0)
		{
			bool spawned = false;
			if (isNight) {
				for (int attempt = 0; attempt < 5 && !spawned; ++attempt) {
					glm::vec3 spawnPos;
					if (!findGroundSpawn(ppos, spawnPos)) continue;
					livingEntities.push_back(std::make_shared<Creeper>(spawnPos));
					creeperCount++;
					spawned = true;
				}
			}
			if (!spawned && creeperCount < MAX_CREEPERS_PER_PLAYER * playerCap) {
				for (int attempt = 0; attempt < 5; ++attempt) {
					glm::vec3 spawnPos;
					if (!findCaveSpawn(ppos, spawnPos)) continue;
					livingEntities.push_back(std::make_shared<Creeper>(spawnPos));
					creeperCount++;
					break;
				}
			}
		}
	}
}
