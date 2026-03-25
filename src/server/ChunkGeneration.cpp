#include "ChunkGeneration.hpp"

ChunkGeneration::ChunkGeneration(const int chunkX, const int chunkZ, const TerrainGenerationParams& params, const bool doGenerate) :
	Chunk(chunkX, chunkZ, 4),
	currentParams(params)  // or more, depending on palette size. We could even use 3 as we use less than 8 types of blocks
{
	if (doGenerate)
    	generate(params);
}

// Returns the warped river noise field in [-1, 1].
float ChunkGeneration::getRiverNoise(const TerrainGenerationParams& terrainParams, float worldX, float worldZ) {
    static Noise riverNoise(terrainParams.seed + 7717);
    static Noise riverWarpX(terrainParams.seed + 7718);
    static Noise riverWarpZ(terrainParams.seed + 7719);

    const float warpX = riverWarpX.fractalBrownianMotion2D(
        worldX * terrainParams.riverWarpFrequency,
        worldZ * terrainParams.riverWarpFrequency,
        3, 2.0f, 0.5f
    ) * terrainParams.riverWarpStrength;

    const float warpZ = riverWarpZ.fractalBrownianMotion2D(
        worldX * terrainParams.riverWarpFrequency,
        worldZ * terrainParams.riverWarpFrequency,
        3, 2.0f, 0.5f
    ) * terrainParams.riverWarpStrength;

    const float wx = worldX + warpX;
    const float wz = worldZ + warpZ;

    return riverNoise.fractalBrownianMotion2D(
        wx * terrainParams.riverFrequency,
        wz * terrainParams.riverFrequency,
        terrainParams.riverOctaves,
        terrainParams.riverLacunarity,
        terrainParams.riverPersistence
    );
}

// Converts river noise into a carving mask [0,1], then filters by
// inlandness, altitude and steepness so rivers prefer low/medium valleys.
float ChunkGeneration::getRiverMask(const TerrainGenerationParams& terrainParams, float worldX, float worldZ, float continentalness, float baseHeight, float pv) {
    const float river = getRiverNoise(terrainParams, worldX, worldZ);

    float riverCenter = 1.0f - glm::smoothstep(
        terrainParams.riverWidth,
        terrainParams.riverWidth + terrainParams.riverBankFeather,
        std::abs(river)
    );
    riverCenter = std::pow(glm::clamp(riverCenter, 0.0f, 1.0f), 1.5f);

    const float inlandMask = glm::smoothstep(
        terrainParams.riverMinContinentalness,
        terrainParams.riverMinContinentalness + 0.24f,
        continentalness
    );

    const float mountainContBlock = 1.0f - glm::smoothstep(
        terrainParams.riverMaxContinentalness - 0.2f,
        terrainParams.riverMaxContinentalness,
        continentalness
    );

    const float lowlandMask = glm::smoothstep(
        static_cast<float>(terrainParams.seaLevel) + 2.0f,
        static_cast<float>(terrainParams.seaLevel) + 30.0f,
        baseHeight
    );

    const float mountainBlock = 1.0f - glm::smoothstep(
        static_cast<float>(terrainParams.seaLevel) + 45.0f,
        static_cast<float>(terrainParams.seaLevel) + 95.0f,
        baseHeight
    );

    const float steepnessMask = 1.0f - glm::smoothstep(0.35f, 0.85f, std::abs(pv));

    return riverCenter * inlandMask * mountainContBlock * lowlandMask * mountainBlock * steepnessMask;
}

// Returns lake base noise mapped to [0,1] for debugging and thresholding.
float ChunkGeneration::getLakeNoise(const TerrainGenerationParams& terrainParams, float worldX, float worldZ) {
    static Noise lakeNoise(terrainParams.seed + 9901);
    const float raw = lakeNoise.fractalBrownianMotion2D(
        worldX * terrainParams.lakeFrequency,
        worldZ * terrainParams.lakeFrequency,
        terrainParams.lakeOctaves,
        terrainParams.lakeLacunarity,
        terrainParams.lakePersistence
    );

    return (raw + 1.0f) * 0.5f;
}

// Converts lake noise into a carving mask [0,1], favoring inland,
// flatter and mid-altitude zones to avoid mountain-top basins.
float ChunkGeneration::getLakeMask(const TerrainGenerationParams& terrainParams, float worldX, float worldZ, float continentalness, float baseHeight, float pv) {
    const float lake01 = getLakeNoise(terrainParams, worldX, worldZ);

    float lakeCore = glm::smoothstep(
        terrainParams.lakeThreshold,
        terrainParams.lakeThreshold + terrainParams.lakeFeather,
        lake01
    );

    const float inlandMask = glm::smoothstep(
        terrainParams.lakeMinContinentalness,
        terrainParams.lakeMinContinentalness + 0.22f,
        continentalness
    );

    const float mountainContBlock = 1.0f - glm::smoothstep(
        terrainParams.lakeMaxContinentalness - 0.15f,
        terrainParams.lakeMaxContinentalness,
        continentalness
    );

    const float flatMask = 1.0f - glm::smoothstep(0.28f, 0.90f, std::abs(pv));
    const float altitudeMask = glm::smoothstep(
        static_cast<float>(terrainParams.seaLevel) + 2.0f,
        static_cast<float>(terrainParams.seaLevel) + 52.0f,
        baseHeight
    );

    return lakeCore * inlandMask * mountainContBlock * flatMask * altitudeMask;
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

            auto top = BlockType::GRASS;
            auto fill = BlockType::DIRT;

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
    generateCacti(blocks, terrainParams);

    generateOres(blocks, terrainParams);
    
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
                    if (b != BlockType::AIR && !oreTypes.contains(b))
                        blocks.at(x, y, z) = BlockType::AIR;
                }
    }

    // encode palette and block data (must be done before vegetation generation)
    blockIndices.encodeAll(blocks.getData(), palette, paletteMap);

    generateVegetation(blocks, terrainParams);
}

// Place a single tree's blocks into the local BlockStorage.
// trunkWorldX/Z is the world-space column of the trunk.
// Only blocks that fall within this chunk's bounds are written.
void ChunkGeneration::placeTree(BlockStorage &blocks, int trunkWorldX, int trunkWorldZ,
                                int surfaceY, int treeHeight) const {
    // Convert trunk world coords to local coords
    const int trunkLocalX = trunkWorldX - originX;
    const int trunkLocalZ = trunkWorldZ - originZ;

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
        const int ly = surfaceY + treeHeight - 2 + layer;
        if (ly < 0 || ly >= HEIGHT)
            continue;

        const int radius = leafRadii[layer];
        for (int lx = -radius; lx <= radius; ++lx) {
            for (int lz = -radius; lz <= radius; ++lz) {
                // Diamond shape: skip corners for radius 2
                if (radius == 2 && abs(lx) == 2 && abs(lz) == 2)
                    continue;

                const int leafLocalX = trunkLocalX + lx;
                const int leafLocalZ = trunkLocalZ + lz;

                if (leafLocalX < 0 || leafLocalX >= WIDTH ||
                    leafLocalZ < 0 || leafLocalZ >= DEPTH)
                    continue;

                if (blocks.at(leafLocalX, ly, leafLocalZ) != BlockType::LOG)
                    blocks.at(leafLocalX, ly, leafLocalZ) = BlockType::LEAVES;
            }
        }
    }
}

void ChunkGeneration::generateTrees(BlockStorage &blocks, const TerrainGenerationParams &terrainParams) const {
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

void ChunkGeneration::generateCacti(BlockStorage &blocks, const TerrainGenerationParams &terrainParams) const {
    const int minWorldX = originX - 1;
    const int maxWorldX = originX + WIDTH + 1;
    const int minWorldZ = originZ - 1;
    const int maxWorldZ = originZ + DEPTH + 1;

    for (int worldX = minWorldX; worldX <= maxWorldX; ++worldX) {
        for (int worldZ = minWorldZ; worldZ <= maxWorldZ; ++worldZ) {

            std::seed_seq seedData{
                static_cast<uint32_t>(terrainParams.seed),
                static_cast<uint32_t>(worldX),
                static_cast<uint32_t>(worldZ),
            };
            std::mt19937 rng(seedData);

            const int surfaceY = computeTerrainHeight(terrainParams,
                static_cast<float>(worldX), static_cast<float>(worldZ));

            if (surfaceY <= terrainParams.seaLevel || surfaceY >= HEIGHT - 5) {
                rng();
                continue;
            }

            const BiomeType biome = computeBiome(terrainParams,
                static_cast<float>(worldX), static_cast<float>(worldZ), surfaceY);
            if (biome != BiomeType::DESERT) {
                rng();
                continue;
            }

            // 0.1% chance per column to place a cactus
            if (rng() % 1000 >= 1)
                continue;

            // Place a 3-block tall cactus column if it can fit in this chunk
            int localX = worldX - originX;
            int localZ = worldZ - originZ;
            if (localX < -1 || localX > WIDTH || localZ < -1 || localZ > DEPTH)
                continue;

            for (int y = surfaceY + 1; y <= surfaceY + 3 && y < HEIGHT; ++y) {
                if (localX >= 0 && localX < WIDTH && localZ >= 0 && localZ < DEPTH) {
                    blocks.at(localX, y, localZ) = BlockType::CACTUS;
                }
            }
        }
    }
}

void ChunkGeneration::generateCaves(BlockStorage &blocks, const TerrainGenerationParams &terrainParams) const {
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
        const auto wx = static_cast<float>(originX + x);
        for (int z = 0; z < Chunk::DEPTH; ++z) {
            const auto wz = static_cast<float>(originZ + z);
            const int surfaceY = surfaceCache[x][z];
            const int caveTopY = std::min(surfaceY, Chunk::HEIGHT - 1);

            for (int y = yStart; y <= caveTopY; ++y) {
                const BlockType cur = blocks.at(x, y, z);
                if (cur == BlockType::AIR || cur == BlockType::WATER ||
                    cur == BlockType::BEDROCK)
                    continue;

                const auto fy = static_cast<float>(y);
                const auto depth = static_cast<float>(surfaceY - y);
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

void ChunkGeneration::generateOres(BlockStorage &blocks, const TerrainGenerationParams &terrainParams) const {
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

int ChunkGeneration::computeTerrainHeight(const TerrainGenerationParams& terrainParams, const float worldX, const float worldZ) {
    // find min/max of the erosion spline
    float eroMin = std::numeric_limits<float>::infinity();
    float eroMax = -std::numeric_limits<float>::infinity();
    for (const auto &p: erosionSpline) { const float val = p.second; eroMin = glm::min(eroMin, val); eroMax = glm::max(eroMax, val); }

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

    float finalHeight = baseHeight - erosionDelta + pvFactor;

    // Carve a smooth two-part river profile: deep core + softer banks.
    const float riverMask = getRiverMask(terrainParams, worldX, worldZ, continentalness, finalHeight, pv);
    if (riverMask > 0.0f) {
        const float bankMask = std::pow(riverMask, 0.45f);
        finalHeight -= terrainParams.riverDepth * 0.85f * riverMask;
        finalHeight -= terrainParams.riverDepth * 0.60f * bankMask;

        if (riverMask > 0.72f) {
            const float t = glm::clamp((riverMask - 0.72f) / 0.28f, 0.0f, 1.0f);
            const float targetBed = static_cast<float>(terrainParams.seaLevel) - 1.5f;
            finalHeight = glm::mix(finalHeight, targetBed, t);
        }
    }

    // Lakes use the same sea level, but with broader/softer basin shaping.
    const float lakeMask = getLakeMask(terrainParams, worldX, worldZ, continentalness, finalHeight, pv);
    if (lakeMask > 0.0f) {
        const float basinMask = std::pow(lakeMask, 0.65f);
        finalHeight -= terrainParams.lakeDepth * 0.90f * lakeMask;
        finalHeight -= terrainParams.lakeDepth * 0.55f * basinMask;

        if (lakeMask > 0.70f) {
            const float t = glm::clamp((lakeMask - 0.70f) / 0.30f, 0.0f, 1.0f);
            const float targetBed = static_cast<float>(terrainParams.seaLevel) - 1.5f;
            finalHeight = glm::mix(finalHeight, targetBed, t);
        }
    }

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

            if (biome == BiomeType::OCEAN) {
                // Ocean vegetation: surface must be sand and above must be water
                if (surfaceBlock != BlockType::SAND)
                    continue;
                if (aboveBlock != BlockType::WATER)
                    continue;
            } else {
                // Land vegetation: must be above sea level
                if (surfaceY <= terrainParams.seaLevel)
                    continue;

                // Don't place vegetation on water, or snow
                if (surfaceBlock == BlockType::WATER ||
                    surfaceBlock == BlockType::SNOW)
                    continue;

                // Only place vegetation on grass, dirt, or sand blocks
                if (surfaceBlock != BlockType::GRASS && surfaceBlock != BlockType::DIRT && surfaceBlock != BlockType::SAND)
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

            // Skip some columns for variety — spawn chance is per-biome
            int spawnChance; // out of 100
            switch (biome) {
                case BiomeType::PLAINS:  spawnChance = 10; break;
                case BiomeType::FOREST:  spawnChance = 5; break;
                case BiomeType::SWAMP:   spawnChance = 0; break;
                case BiomeType::OCEAN:   spawnChance = 20; break;
                case BiomeType::DESERT:  spawnChance = 1;  break;
                default:                 spawnChance = 10; break;
            }
            if (static_cast<int>(rng() % 100) >= spawnChance)
                continue;

            // Weighted vegetation tables per biome
            // For example, weight 1 among a total of ~200 gives a ~0.5% chance per spawn
            struct VegEntry { BlockType type; int weight; };

            static constexpr VegEntry plainsVeg[] = {
                { BlockType::SHORT_GRASS,           100 },
                { BlockType::POPPY,                 1 },
                { BlockType::CORNFLOWER,            1 },
                { BlockType::PINK_TULIP,            1 },
                { BlockType::ORANGE_TULIP,          1 },
                { BlockType::RED_TULIP,             1 },
                { BlockType::WHITE_TULIP,           1 },
                { BlockType::BLUE_ORCHID,           1 },
                { BlockType::LILY_OF_THE_VALLEY,    1 },
                { BlockType::WITHER_ROSE,           1 },
                { BlockType::DANDELION,             1 },
                { BlockType::ALLIUM,                1 },
                { BlockType::AZURE_BLUET,           1 },
                { BlockType::OXEYE_DAISY,           1 }
            };

            static constexpr VegEntry forestVeg[] = {
                { BlockType::SHORT_GRASS,           50 },
                { BlockType::BROWN_MUSHROOM,        50 },
                { BlockType::RED_MUSHROOM,          50 },
            };

            static constexpr VegEntry desertVeg[] = {
                { BlockType::DEAD_BUSH,             100 },
            };

            // static const VegEntry swampVeg[] = {
            //     // { BlockType::SHORT_GRASS, 100 },
            //     // { BlockType::TALL_GRASS, 80 },
            // };

            static const VegEntry oceanVeg[] = {
                { BlockType::KELP,                  100 },
                { BlockType::SEAGRASS,              10 },
                { BlockType::TALL_SEAGRASS_BOTTOM,  60 },
                { BlockType::BRAIN_CORAL,           10 },
                { BlockType::BRAIN_CORAL_FAN,       10 },
                { BlockType::BUBBLE_CORAL,          10 },
                { BlockType::BUBBLE_CORAL_FAN,      10 },
                { BlockType::FIRE_CORAL,            10 },
                { BlockType::FIRE_CORAL_FAN,        10 },
                { BlockType::HORN_CORAL,            10 },
                { BlockType::HORN_CORAL_FAN,        10 },
                { BlockType::TUBE_CORAL,            10 },
                { BlockType::TUBE_CORAL_FAN,        10 },
            };

            // Pick the table for this biome
            const VegEntry* vegTable = nullptr;
            int vegTableSize = 0;

            switch (biome) {
                case BiomeType::PLAINS:
                    vegTable = plainsVeg;
                    vegTableSize = std::size(plainsVeg);
                    break;
                case BiomeType::FOREST:
                    vegTable = forestVeg;
                    vegTableSize = std::size(forestVeg);
                    break;
                // case BiomeType::SWAMP:
                //     vegTable = swampVeg;
                //     vegTableSize = sizeof(swampVeg) / sizeof(swampVeg[0]);
                //     break;
                case BiomeType::OCEAN:
                    vegTable = oceanVeg;
                    vegTableSize = std::size(oceanVeg);
                    break;
                case BiomeType::DESERT:
                    vegTable = desertVeg;
                    vegTableSize = std::size(desertVeg);
                    break;
                default:
                    continue; // No vegetation in, tundra, mountain
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
                    vegType == BlockType::BRAIN_CORAL_FAN || vegType == BlockType::BUBBLE_CORAL_FAN ||
                    vegType == BlockType::FIRE_CORAL || vegType == BlockType::FIRE_CORAL_FAN ||
                    vegType == BlockType::HORN_CORAL || vegType == BlockType::HORN_CORAL_FAN ||
                    vegType == BlockType::TUBE_CORAL || vegType == BlockType::TUBE_CORAL_FAN
                    ) {
                    // Simple seagrass/coral: place a single instance
                    VegetationInstance veg{};
                    veg.x = static_cast<uint8_t>(x);
                    veg.y = static_cast<uint8_t>(surfaceY + 1);
                    veg.z = static_cast<uint8_t>(z);
                    veg.type = vegType;
                    vegetation.push_back(veg);
                } else {
                    // Kelp or tall seagrass: stack multiple instances
                    const int waterDepth = terrainParams.seaLevel - surfaceY;
                    constexpr int maxHeight = std::max(2, 10);
                    int height = 2 + static_cast<int>(rng() % std::max(1, maxHeight - 1));
                    height = std::min(height, waterDepth - 1); // don't poke above water

                    const bool isKelp = (vegType == BlockType::KELP);

                    for (int dy = 1; dy <= height; ++dy) {
                        VegetationInstance veg{};
                        veg.x = static_cast<uint8_t>(x);
                        veg.y = static_cast<uint8_t>(surfaceY + dy);
                        veg.z = static_cast<uint8_t>(z);

                        const bool isTop = (dy == height);
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
                VegetationInstance veg{};
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
