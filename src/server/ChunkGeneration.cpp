
#include "ChunkGeneration.hpp"

ChunkGeneration::ChunkGeneration(const int chunkX, const int chunkZ, const TerrainGenerationParams& params, const bool doGenerate) :
	Chunk(chunkX, chunkZ, 4),
	currentParams(params)  // or more, depending on palette size. We could even use 3 as we use less than 8 types of blocks
{
	if (doGenerate)
    	generate(params);
}

void ChunkGeneration::generate(const TerrainGenerationParams& terrainParams) {

    // local storage
    BlockStorage blocks;

    for (int x = 0; x < WIDTH; ++x) {
        for (int z = 0; z < DEPTH; ++z) {
            const auto worldX = static_cast<float>(originX + x);
            const auto worldZ = static_cast<float>(originZ + z);

            const int surfaceY = computeTerrainHeight(terrainParams, worldX, worldZ);

            #ifndef NDEBUG
            if (surfaceY < 0 || surfaceY >= HEIGHT) {
                std::cerr << "Invalid surfaceY: " << surfaceY << " at world (" << worldX << "," << worldZ << ")\n";
            }
            #endif

            BlockType top = BlockType::GRASS;
            BlockType fill = BlockType::DIRT;

            // SAND at sea levels
            if (surfaceY < terrainParams.seaLevel) {
                fill = BlockType::SAND;
                top = BlockType::SAND;
            }

            // Bedrock base
            for (int y = 0; y <= terrainParams.bedrockLevel; ++y)
                blocks.at(x, y, z) = BlockType::BEDROCK;

            // Terrain shaping
            for (int y = terrainParams.bedrockLevel + 1; y < surfaceY; ++y)
                blocks.at(x, y, z) = BlockType::STONE;

            // Air above
            for (int y = std::max(terrainParams.seaLevel + 1, surfaceY + 1); y < HEIGHT; ++y)
                blocks.at(x, y, z) = BlockType::AIR;

            // Compute biome
            const BiomeType biome = computeBiome(terrainParams, worldX, worldZ, surfaceY);

            // Set blocks based on biome
            for (int y = std::max(terrainParams.bedrockLevel + 1, surfaceY - 3); y < surfaceY && y < HEIGHT; y++) {
                switch (biome) {
                    case BiomeType::DESERT:
                        top = fill = BlockType::SAND;
                        break;
                    case BiomeType::TUNDRA:
                        top = BlockType::SNOW;
                        fill = BlockType::DIRT;
                        break;
                    case BiomeType::FOREST: {
                        top = BlockType::GRASS;
                        fill = BlockType::DIRT;
                        break;
                    }
                        
                    case BiomeType::SWAMP: top = fill = BlockType::SAND; break;
                    case BiomeType::MOUNTAIN: top = fill = BlockType::STONE; break;
                    default:
                        top = BlockType::GRASS;
                        fill = BlockType::DIRT; // PLAINS
                }

                blocks.at(x, y, z) = fill;
            }

            // Water up to sea levels
            for (int y = surfaceY; y <= terrainParams.seaLevel && y < HEIGHT; ++y)
			{
                blocks.at(x, y, z) = BlockType::WATER;
			}

            // Set top block only if above water
            if (surfaceY > terrainParams.seaLevel) {
                blocks.at(x, surfaceY, z) = top;
            } else {
                blocks.at(x, surfaceY, z) = BlockType::SAND;
            }

        }
    }

    generateCaves(blocks, terrainParams);

    generateTrees(blocks, terrainParams);

    generateOres(blocks, terrainParams);

    // encode palette and block data (must be done before vegetation generation)
    blockIndices.encodeAll(blocks.getData(), palette, paletteMap);

    generateVegetation(blocks, terrainParams);

    // DEBUG: strip everything except ores so they're visible in isolation
    if (terrainParams.debugOresOnly) {
        // Build a set of ore block types for fast lookup
        std::unordered_set<BlockType> oreTypes;
        for (const auto &ore : oreTable)
            oreTypes.insert(ore.type);

        for (int x = 0; x < WIDTH; ++x)
            for (int y = 0; y < HEIGHT; ++y)
                for (int z = 0; z < DEPTH; ++z) {
                    BlockType b = blocks.at(x, y, z);
                    if (b != BlockType::AIR && oreTypes.find(b) == oreTypes.end())
                        blocks.at(x, y, z) = BlockType::AIR;
                }
    }

}

// Place a single tree's blocks into the local BlockStorage.
// trunkWorldX/Z is the world-space column of the trunk.
// Only blocks that fall within this chunk's bounds are written.
void ChunkGeneration::placeTree(BlockStorage &blocks, int trunkWorldX, int trunkWorldZ,
                                int surfaceY, int treeHeight) {
    // Convert trunk world coords to local coords
    int trunkLocalX = trunkWorldX - originX;
    int trunkLocalZ = trunkWorldZ - originZ;

    // Place dirt under the tree (only if trunk is inside this chunk)
    if (trunkLocalX >= 0 && trunkLocalX < WIDTH &&
        trunkLocalZ >= 0 && trunkLocalZ < DEPTH) {
        blocks.at(trunkLocalX, surfaceY, trunkLocalZ) = BlockType::DIRT;
    }

    // Trunk
    if (trunkLocalX >= 0 && trunkLocalX < WIDTH &&
        trunkLocalZ >= 0 && trunkLocalZ < DEPTH) {
        for (int treeY = surfaceY + 1; treeY <= surfaceY + treeHeight && treeY < HEIGHT; ++treeY) {
            blocks.at(trunkLocalX, treeY, trunkLocalZ) = BlockType::LOG;
        }
    }

    // Leaves – 4 layers with widths 5-5-3-1 (bottom to top)
    // Layer 0 (bottom): radius 2, Layer 1: radius 2, Layer 2: radius 1, Layer 3 (top): radius 0
    constexpr int leafRadii[4] = {2, 2, 1, 0};

    for (int layer = 0; layer < 4; ++layer) {
        int ly = surfaceY + treeHeight - 2 + layer;
        if (ly < 0 || ly >= HEIGHT)
            continue;

        int radius = leafRadii[layer];
        for (int lx = -radius; lx <= radius; ++lx) {
            for (int lz = -radius; lz <= radius; ++lz) {
                // Diamond shape: skip corners for radius 2
                if (radius == 2 && abs(lx) == 2 && abs(lz) == 2)
                    continue;

                int leafLocalX = trunkLocalX + lx;
                int leafLocalZ = trunkLocalZ + lz;

                if (leafLocalX < 0 || leafLocalX >= WIDTH ||
                    leafLocalZ < 0 || leafLocalZ >= DEPTH)
                    continue;

                if (blocks.at(leafLocalX, ly, leafLocalZ) != BlockType::LOG)
                    blocks.at(leafLocalX, ly, leafLocalZ) = BlockType::LEAVES;
            }
        }
    }
}

void ChunkGeneration::generateTrees(BlockStorage &blocks, const TerrainGenerationParams &terrainParams) {
    // Tree leaves extend up to 2 blocks horizontally. To handle trees from
    // neighboring chunks whose canopy spills into this chunk, we iterate
    // over the current chunk and all 8 neighbors' tree positions.

    // The maximum horizontal reach of a tree canopy in blocks.
    constexpr int TREE_REACH = 2;

    // Iterate only over columns whose trees can reach into this chunk:
    // expand the area by TREE_REACH in all directions around [originX, originZ].
    const int minWorldX = originX - TREE_REACH;
    const int maxWorldX = originX + WIDTH + TREE_REACH;
    const int minWorldZ = originZ - TREE_REACH;
    const int maxWorldZ = originZ + DEPTH + TREE_REACH;

    for (int worldX = minWorldX; worldX <= maxWorldX; ++worldX) {
        for (int worldZ = minWorldZ; worldZ <= maxWorldZ; ++worldZ) {

            // Deterministic RNG seeded per world column
            std::seed_seq seedData{
                static_cast<uint32_t>(terrainParams.seed),
                static_cast<uint32_t>(worldX),
                static_cast<uint32_t>(worldZ)
            };
            std::mt19937 rng(seedData);

            const int surfaceY = computeTerrainHeight(terrainParams,
                static_cast<float>(worldX), static_cast<float>(worldZ));

            // Only place trees above sea level
            if (surfaceY <= terrainParams.seaLevel || surfaceY >= HEIGHT - 12) {
                // Still advance the RNG to keep determinism
                rng(); // for the tree chance roll
                continue;
            }

            const BiomeType biome = computeBiome(terrainParams,
                static_cast<float>(worldX), static_cast<float>(worldZ), surfaceY);
            if (biome != BiomeType::FOREST) {
                rng();
                continue;
            }

            // 1% chance per column to place a tree
            if (rng() % 1000 >= 10)
                continue;

            int treeHeight = 4 + static_cast<int>(rng() % 7);

            // Quick check: can any part of this tree reach into our chunk?
            int localTrunkX = worldX - originX;
            int localTrunkZ = worldZ - originZ;
            if (localTrunkX < -TREE_REACH || localTrunkX >= WIDTH + TREE_REACH ||
                localTrunkZ < -TREE_REACH || localTrunkZ >= DEPTH + TREE_REACH)
                continue;

            placeTree(blocks, worldX, worldZ, surfaceY, treeHeight);
        }
    }
}

void ChunkGeneration::generateCaves(BlockStorage &blocks, const TerrainGenerationParams &terrainParams) {
    // Maybe check based on biome or something to skip cave generation for some biomes (e.g. ocean)
    const int yStart = terrainParams.bedrockLevel + 3;

    // Cache surface height per column
    int surfaceCache[Chunk::WIDTH][Chunk::DEPTH];
    for (int x = 0; x < Chunk::WIDTH; ++x) {
        const auto worldX = static_cast<float>(originX + x);
        for (int z = 0; z < Chunk::DEPTH; ++z) {
            const auto worldZ = static_cast<float>(originZ + z);
            surfaceCache[x][z] = computeTerrainHeight(terrainParams, worldX, worldZ);
        }
    }

    static Noise noiseA(terrainParams.seed + 7890);
    static Noise noiseB(terrainParams.seed + 4561);

    // Tuning – Perlin3D output is ~[-0.7, 0.7], so thresholds must be tight
    constexpr float spagScaleH    = 0.01f;       // horizontal frequency
    constexpr float spagScaleV    = 0.01f;       // vertical frequency
    constexpr float threshDeep    = 0.025f;      // deep underground threshold
    constexpr float threshSurface = 0.01f;      // narrow surface entrances
    constexpr float fadeBlocks    = 4.0f;
    // Second field at different scale to break regularity
    constexpr float bScaleHMul    = 1.4f;
    constexpr float bScaleVMul    = 2.2f;

    for (int x = 0; x < Chunk::WIDTH; ++x) {
        const float wx = static_cast<float>(originX + x);
        for (int z = 0; z < Chunk::DEPTH; ++z) {
            const float wz = static_cast<float>(originZ + z);
            const int surfaceY = surfaceCache[x][z];
            const int caveTopY = std::min(surfaceY, Chunk::HEIGHT - 1);

            for (int y = yStart; y <= caveTopY; ++y) {
                const BlockType cur = blocks.at(x, y, z);
                if (cur == BlockType::AIR || cur == BlockType::WATER ||
                    cur == BlockType::BEDROCK)
                    continue;

                const float fy = static_cast<float>(y);
                const float depth = static_cast<float>(surfaceY - y);
                const float depthFade = glm::smoothstep(0.0f, fadeBlocks, depth);
                const float thresh = threshSurface + depthFade * (threshDeep - threshSurface);

                const float nA = noiseA.perlin3D(
                    wx * spagScaleH, fy * spagScaleV, wz * spagScaleH);
                const float nB = noiseB.perlin3D(
                    wx * spagScaleH * bScaleHMul,
                    fy * spagScaleV * bScaleVMul,
                    wz * spagScaleH * bScaleHMul);

                if (std::abs(nA) < thresh && std::abs(nB) < thresh) {
                    // Don't carve if the block directly above is water (prevent water flooding)
                    if (y + 1 < Chunk::HEIGHT && blocks.at(x, y + 1, z) == BlockType::WATER)
                        continue;
                    blocks.at(x, y, z) = BlockType::AIR;
                }
            }
        }
    }
}

void ChunkGeneration::generateOres(BlockStorage &blocks, const TerrainGenerationParams &terrainParams) {
    for (const auto &ore : oreTable) {
        // Deterministic RNG per ore type per chunk
        std::seed_seq seedData{
            static_cast<uint32_t>(terrainParams.seed),
            static_cast<uint32_t>(originX),
            static_cast<uint32_t>(originZ),
            static_cast<uint32_t>(ore.type)
        };
        std::mt19937 rng(seedData);

        std::uniform_int_distribution<int> distX(0, WIDTH - 1);
        std::uniform_int_distribution<int> distZ(0, DEPTH - 1);
        std::uniform_int_distribution<int> distY(ore.minY, ore.maxY);
        std::uniform_int_distribution<int> spread(-1, 1);

        for (int v = 0; v < ore.veinsPerChunk; ++v) {
            int cx = distX(rng);
            int cy = distY(rng);
            int cz = distZ(rng);

            // Only place ore where there is already stone
            for (int i = 0; i < ore.veinSize; ++i) {
                // Clamp to chunk bounds
                int bx = std::clamp(cx, 0, WIDTH - 1);
                int by = std::clamp(cy, 0, HEIGHT - 1);
                int bz = std::clamp(cz, 0, DEPTH - 1);

                if (blocks.at(bx, by, bz) == BlockType::STONE) {
                    blocks.at(bx, by, bz) = ore.type;
                }

                // Random walk to next block in vein
                cx += spread(rng);
                cy += spread(rng);
                cz += spread(rng);
            }
        }
    }
}


// Linear interpolation between spline points
float ChunkGeneration::interpolateSpline(float noise, const std::vector<std::pair<float, float>>& spline) {
    const auto& pts = spline;
    if (noise <= pts.front().first) return pts.front().second;
    if (noise >= pts.back().first) return pts.back().second;

    // Find the interval
    for (size_t i = 1; i < pts.size(); ++i) {
        if (noise < pts[i].first) {
            float t = (noise - pts[i-1].first) / (pts[i].first - pts[i-1].first);
            return pts[i-1].second + t * (pts[i].second - pts[i-1].second);
        }
    }
    return pts.back().second; // fallback
}

float ChunkGeneration::getContinentalness(const TerrainGenerationParams& terrainParams, float wx, float wz) {
    static Noise baseNoise(terrainParams.seed);

    float fbm = baseNoise.fractalBrownianMotion2D(
        wx * terrainParams.continentalnessFrequency,
        wz * terrainParams.continentalnessFrequency,
        terrainParams.continentalnessOctaves,
        terrainParams.continentalnessLacunarity,
        terrainParams.continentalnessPersistence
        );

    fbm *= terrainParams.continentalnessScalingFactor;
    
    float continentalness = glm::clamp(fbm, -3.8f, 3.8f);

    return continentalness;
}

float ChunkGeneration::getErosion(const TerrainGenerationParams& terrainParams, float wx, float wz) {
    static Noise erosionNoise(terrainParams.seed + 237);

    float erosion = erosionNoise.fractalBrownianMotion2D(
        wx * terrainParams.erosionFrequency,
        wz * terrainParams.erosionFrequency,
        terrainParams.erosionOctaves,
        terrainParams.erosionLacunarity,
        terrainParams.erosionPersistence
        );

    erosion = glm::clamp(erosion * terrainParams.erosionScalingFactor, -1.0f, 1.0f);
    return erosion;
}

float ChunkGeneration::getPV(const TerrainGenerationParams& terrainParams, float wx, float wz) {
    static Noise peakValleyNoise(terrainParams.seed + 98789);

    float peakValley = peakValleyNoise.fractalBrownianMotion2D(
        wx * terrainParams.peakValleyFrequency,
        wz * terrainParams.peakValleyFrequency,
        terrainParams.peakValleyOctaves,
        terrainParams.peakValleyLacunarity,
        terrainParams.peakValleyPersistence
        );

    peakValley = glm::clamp(peakValley * terrainParams.peakValleyScalingFactor, -1.0f, 1.0f);

    return peakValley;
}

float ChunkGeneration::getTemperature(const TerrainGenerationParams& terrainParams, float wx, float wz) {
    static Noise tempNoise(terrainParams.seed + 123);

    float temperature = tempNoise.fractalBrownianMotion2D(
        wx * terrainParams.temperatureFrequency,
        wz * terrainParams.temperatureFrequency,
        terrainParams.temperatureOctaves,
        terrainParams.temperatureLacunarity,
        terrainParams.temperaturePersistence
        ) * terrainParams.temperatureScalingFactor;

    return temperature;
}

float ChunkGeneration::getHumidity(const TerrainGenerationParams& terrainParams, float wx, float wz) {
    static Noise humidNoise(terrainParams.seed + 456);

    float humidity = humidNoise.fractalBrownianMotion2D(
        wx * terrainParams.humidityFrequency,
        wz * terrainParams.humidityFrequency,
        terrainParams.humidityOctaves,
        terrainParams.humidityLacunarity,
        terrainParams.humidityPersistence
        ) * terrainParams.humidityScalingFactor;

    return humidity;
}

float ChunkGeneration::surfaceNoiseTransformation(float noise, int splineIndex) {
    float noiseTransform = 0.0f;
    if (splineIndex == 1)
        noiseTransform = interpolateSpline(noise, continentalnessSpline);
    else if (splineIndex == 2)
        noiseTransform = interpolateSpline(noise, erosionSpline);
    else if (splineIndex == 3)
        noiseTransform = interpolateSpline(noise, peakValleySpline);

    return noiseTransform;
}

BiomeType ChunkGeneration::computeBiome(const TerrainGenerationParams& terrainParams, float worldX, float worldZ, int height) {

    // Build very low-frequency (coarse) climate fields so biomes form large contiguous regions.
    // biomeScaleChunks controls how many chunks make up a biome patch; use an extra multiplier to ensure broad bands.
    if (height <= terrainParams.seaLevel) return BiomeType::OCEAN;

    static Noise tempNoise(terrainParams.seed + 45);
    static Noise humidNoise(terrainParams.seed + 964);

    const float chunks = glm::max(1, terrainParams.biomeScaleChunks);
    const float worldUnitsPerPatch = chunks * Chunk::WIDTH * 8.0f;
    const float freqCoarse = 1.0f / glm::max(256.0f, worldUnitsPerPatch);

    // Coarse climate fields in [0..1]
    float tempCoarse  = (tempNoise.fractalBrownianMotion2D(worldX * freqCoarse,            worldZ * freqCoarse,            4, 2.0f, 0.5f) + 1.0f) * 0.5f;
    float humidCoarse = (humidNoise.fractalBrownianMotion2D(worldX * freqCoarse * 0.9f,    worldZ * freqCoarse * 0.9f,    4, 2.0f, 0.5f) + 1.0f) * 0.5f;

    // Small regional bias
    static Noise regionBias(terrainParams.seed + 4242);
    float bias = (regionBias.fractalBrownianMotion2D(worldX * freqCoarse * 0.6f, worldZ * freqCoarse * 0.6f, 3, 2.0f, 0.5f) + 1.0f) * 0.5f;

    float climate = glm::clamp(glm::mix(tempCoarse, 1.0f - humidCoarse, 0.35f) * 0.7f + bias * 0.3f, 0.0f, 1.0f);


    climate = glm::clamp((climate - 0.5f) * 1.2f + 0.5f, 0.0f, 1.0f);

    // High, cold overrides
    // if (height > terrainParams.seaLevel + 28) {
        if (tempCoarse < terrainParams.snowTemperatureThreshold) return BiomeType::TUNDRA;
        // return BiomeType::MOUNTAIN;
    // }
    float pv = getPV(terrainParams, worldX, worldZ);
    // --- DESERT: hot + dry, inland, mid elevations ---
    float aridity = (1.0f - humidCoarse) * tempCoarse;
    if (aridity > 0.3f &&
        tempCoarse > 0.40f &&
        humidCoarse < 0.45f &&
        height <= 90 && pv < 0.2f
        ){
        return BiomeType::DESERT;
        }

    // Cold lowlands
    if (climate < 0.16f) return BiomeType::TUNDRA;
    if (height > terrainParams.seaLevel + 30 && tempCoarse < 0.45f) return BiomeType::TUNDRA;

    // SWAMP: wet, low-lying, mild temps
    if (height <= terrainParams.seaLevel + 6 &&
        humidCoarse > 0.60f &&
        tempCoarse > 0.30f && tempCoarse < 0.80f) {
        return BiomeType::SWAMP;
    }

    // Forest: moist and not too hot
    if (humidCoarse > terrainParams.forestMoistureThreshold * 0.9f && climate < 0.65f) return BiomeType::FOREST;

    return BiomeType::PLAINS;


}

int ChunkGeneration::computeTerrainHeight(const TerrainGenerationParams& terrainParams, float worldX, float worldZ) {
    // find min/max of the erosion spline
    float eroMin = std::numeric_limits<float>::infinity();
    float eroMax = -std::numeric_limits<float>::infinity();
    for (const auto &p : erosionSpline) { eroMin = glm::min(eroMin, p.second); eroMax = glm::max(eroMax, p.second); }

    const float continentalness = getContinentalness(terrainParams, worldX, worldZ);
    const float erosion = getErosion(terrainParams, worldX, worldZ);
    const float pv = getPV(terrainParams, worldX, worldZ);

    const float baseHeight = surfaceNoiseTransformation(continentalness, 1);
    const float erosionSplineValue = surfaceNoiseTransformation(erosion, 2);
    const float pvSplineValue = surfaceNoiseTransformation(pv, 3);

    float erosionNorm = 0.0f;
    if (eroMax > eroMin) erosionNorm = glm::clamp((erosionSplineValue - eroMin) / (eroMax - eroMin), 0.0f, 1.0f);

    erosionNorm = 1.0f - erosionNorm;

    // Modulate erosion strength by location: coasts should erode less, inland/mountains more
    const float inlandMask = glm::smoothstep(-0.19f, 3.8f, continentalness); // 0 = near coast, 1 = inland
    // tune min/max erosion in world units (small compared to absolute heights from continentalness spline)
    constexpr float minErosionStrength = 2.0f;
    constexpr float maxErosionStrength = 140.0f;
    const float erosionStrength = glm::mix(minErosionStrength, maxErosionStrength, inlandMask);

    // Combine normalized spline severity with strength to get final height delta
    const float erosionDelta = erosionNorm * erosionStrength;

    const float pvFactor = pvSplineValue * (1.0f - erosionNorm);

    const float finalHeight = baseHeight - erosionDelta + pvFactor;


    int surfaceY = static_cast<int>(std::floor(finalHeight)); // round
    surfaceY = glm::clamp(surfaceY, 0, HEIGHT - 1);

    return surfaceY;
}

void ChunkGeneration::generateVegetation(const BlockStorage &blocks, const TerrainGenerationParams &terrainParams) {
    // Generate grass and flowers on suitable surface blocks
    // Only place vegetation on solid blocks that are not water/sand/snow
    // Vegetation should be above sea level

    vegetation.clear();

    for (int x = 0; x < WIDTH; ++x) {
        for (int z = 0; z < DEPTH; ++z) {
            const auto worldX = static_cast<float>(originX + x);
            const auto worldZ = static_cast<float>(originZ + z);

            const int surfaceY = computeTerrainHeight(terrainParams, worldX, worldZ);

            if (surfaceY >= HEIGHT - 1)
                continue;

            // Check the block at surface level
            const BlockType surfaceBlock = blocks.at(x, surfaceY, z);
            const BlockType aboveBlock = blocks.at(x, surfaceY + 1, z);

            const BiomeType biome = computeBiome(terrainParams, worldX, worldZ, surfaceY);

            const bool isOcean = (biome == BiomeType::OCEAN);

            if (isOcean) {
                // Ocean vegetation: surface must be sand and above must be water
                if (surfaceBlock != BlockType::SAND)
                    continue;
                if (aboveBlock != BlockType::WATER)
                    continue;
            } else {
                // Land vegetation: must be above sea level
                if (surfaceY <= terrainParams.seaLevel)
                    continue;

                // Don't place vegetation on water, sand, or snow
                if (surfaceBlock == BlockType::WATER || surfaceBlock == BlockType::SAND ||
                    surfaceBlock == BlockType::SNOW)
                    continue;

                // Only place vegetation on grass or dirt blocks
                if (surfaceBlock != BlockType::GRASS && surfaceBlock != BlockType::DIRT)
                    continue;

                // Don't place if there's already something above (like a tree)
                if (aboveBlock != BlockType::AIR)
                    continue;
            }

            // Deterministic RNG seeded per world column
            std::seed_seq seedData{
                static_cast<uint32_t>(terrainParams.seed),
                static_cast<uint32_t>(static_cast<int>(worldX)),
                static_cast<uint32_t>(static_cast<int>(worldZ))
            };
            std::mt19937 rng(seedData);

            // Skip some columns for variety (10% chance to place vegetation)
            if (rng() % 100 >= 10)
                continue;

            // Weighted vegetation tables per biome
            struct VegEntry { BlockType type; int weight; };

            static const VegEntry plainsVeg[] = {
                { BlockType::SHORT_GRASS,           70 },
                { BlockType::POPPY,                 10 },
                { BlockType::CORNFLOWER,            10 },
                { BlockType::PINK_TULIP,            10 },
                { BlockType::ORANGE_TULIP,          10 },
                { BlockType::RED_TULIP,             10 },
                { BlockType::WHITE_TULIP,           10 },
                { BlockType::BLUE_ORCHID,           10 },
                { BlockType::LILY_OF_THE_VALLEY,    10 },
                { BlockType::WITHER_ROSE,           10 },
                { BlockType::DANDELION,             10 },
                { BlockType::ALLIUM,                10 },
                { BlockType::AZURE_BLUET,           10 },
                { BlockType::OXEYE_DAISY,           10 }
            };

            static const VegEntry forestVeg[] = {
                { BlockType::SHORT_GRASS,           60 },
                { BlockType::BROWN_MUSHROOM,        40 },
                { BlockType::RED_MUSHROOM,          40 },
            };

            static const VegEntry swampVeg[] = {
                // { BlockType::SHORT_GRASS, 100 },
                // { BlockType::TALL_GRASS, 80 },
            };

            static const VegEntry oceanVeg[] = {
                { BlockType::KELP, 100 },
                { BlockType::SEAGRASS, 80 },
                { BlockType::TALL_SEAGRASS_BOTTOM, 60 },
                { BlockType::BRAIN_CORAL, 80 },
                { BlockType::BRAIN_CORAL_FAN, 80 },
                { BlockType::BUBBLE_CORAL, 80 },
                { BlockType::BUBBLE_CORAL_FAN, 80 },
            };

            // Pick the table for this biome
            const VegEntry* vegTable = nullptr;
            int vegTableSize = 0;

            switch (biome) {
                case BiomeType::PLAINS:
                    vegTable = plainsVeg;
                    vegTableSize = sizeof(plainsVeg) / sizeof(plainsVeg[0]);
                    break;
                case BiomeType::FOREST:
                    vegTable = forestVeg;
                    vegTableSize = sizeof(forestVeg) / sizeof(forestVeg[0]);
                    break;
                case BiomeType::SWAMP:
                    vegTable = swampVeg;
                    vegTableSize = sizeof(swampVeg) / sizeof(swampVeg[0]);
                    break;
                case BiomeType::OCEAN:
                    vegTable = oceanVeg;
                    vegTableSize = sizeof(oceanVeg) / sizeof(oceanVeg[0]);
                    break;
                default:
                    continue; // No vegetation in desert, tundra, mountain
            }

            // Weighted random pick from the table
            int totalWeight = 0;
            for (int i = 0; i < vegTableSize; ++i)
                totalWeight += vegTable[i].weight;

            int roll = rng() % totalWeight;
            BlockType vegType = vegTable[0].type;
            for (int i = 0; i < vegTableSize; ++i) {
                roll -= vegTable[i].weight;
                if (roll < 0) {
                    vegType = vegTable[i].type;
                    break;
                }
            }

            // Add vegetation instance
            if (biome == BiomeType::OCEAN && surfaceY >= terrainParams.seaLevel - 2)
                continue;

            if (isSeaVegetation(vegType)) {
                if (vegType == BlockType::SEAGRASS || 
                    vegType == BlockType::BRAIN_CORAL || vegType == BlockType::BUBBLE_CORAL || 
                    vegType == BlockType::BRAIN_CORAL_FAN || vegType == BlockType::BUBBLE_CORAL_FAN) {
                    // Simple seagrass/coral: place a single instance
                    VegetationInstance veg;
                    veg.x = static_cast<uint8_t>(x);
                    veg.y = static_cast<uint8_t>(surfaceY + 1);
                    veg.z = static_cast<uint8_t>(z);
                    veg.type = vegType;
                    vegetation.push_back(veg);
                } else {
                    // Kelp or tall seagrass: stack multiple instances
                    int waterDepth = terrainParams.seaLevel - surfaceY;
                    int maxHeight = std::max(2, waterDepth - 1);
                    int height = 2 + static_cast<int>(rng() % std::max(1, maxHeight - 1));
                    height = std::min(height, waterDepth - 1); // don't poke above water

                    bool isKelp = (vegType == BlockType::KELP);

                    for (int dy = 1; dy <= height; ++dy) {
                        VegetationInstance veg;
                        veg.x = static_cast<uint8_t>(x);
                        veg.y = static_cast<uint8_t>(surfaceY + dy);
                        veg.z = static_cast<uint8_t>(z);

                        bool isTop = (dy == height);
                        if (isKelp) {
                            veg.type = isTop ? BlockType::KELP : BlockType::KELP_PLANT;
                        } else {
                            // Tall seagrass
                            veg.type = isTop ? BlockType::TALL_SEAGRASS_TOP : BlockType::TALL_SEAGRASS_BOTTOM;
                        }
                        vegetation.push_back(veg);
                    }
                }
                // Don't store in block grid — keep water blocks intact
            } else {
                VegetationInstance veg;
                veg.x = static_cast<uint8_t>(x);
                veg.y = static_cast<uint8_t>(surfaceY + 1); // Place above surface
                veg.z = static_cast<uint8_t>(z);
                veg.type = vegType;
                vegetation.push_back(veg);

                // Also store in block grid so raycasting can target it
                setBlock(x, surfaceY + 1, z, vegType);
            }
        }
    }
}
