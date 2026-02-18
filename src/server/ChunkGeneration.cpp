
#include "ChunkGeneration.hpp"

ChunkGeneration::ChunkGeneration(const int chunkX, const int chunkZ, const TerrainGenerationParams& params, const bool doGenerate) :
	Chunk(chunkX, chunkZ, 4),
	currentParams(params)  // or more, depending on palette size. We could even use 3 as we use less than 8 types of blocks
{
	if (doGenerate)
    	generate(params);
	else preGenerated = true;
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
                        // Add trees randomly
                        std::seed_seq seedData{terrainParams.seed, x, y, z};
                        std::mt19937 rng(seedData);
                        if (rng() % 1000 < 10) { // 1% chance to add a tree
                            int treeHeight = 4 + rng() % 7; // Random height between 4 and 10
                            top = BlockType::DIRT;
                            for (int treeY = surfaceY; treeY < surfaceY + treeHeight; treeY++) {
                                if (treeY >= 0 && treeY < HEIGHT)
                                    blocks.at(x, treeY, z) = BlockType::LOG;
                            }
                            for (int lx = -2; lx <= 2; lx++) {
                                for (int lz = -2; lz <= 2; lz++) {
                                    for (int ly = surfaceY + treeHeight - 1; ly <= surfaceY + treeHeight + 1; ly++) {
                                        int leafX = x + lx;
                                        int leafY = ly;
                                        int leafZ = z + lz;
                                        if (abs(lx) + abs(lz) <= 3 &&
                                            leafX >= 0 && leafX < WIDTH &&
                                            leafY >= 0 && leafY < HEIGHT &&
                                            leafZ >= 0 && leafZ < DEPTH) {
                                            blocks.at(leafX, leafY, leafZ) = BlockType::LEAVES;
                                        }
                                    }
                                }
                            }
                        }
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
                    if (b != BlockType::AIR && oreTypes.find(b) == oreTypes.end())
                        blocks.at(x, y, z) = BlockType::AIR;
                }
    }

    // encode palette and block data (same as before)
    blockIndices.encodeAll(blocks.getData(), palette, paletteMap);
}

void ChunkGeneration::generateCaves(BlockStorage &blocks, const TerrainGenerationParams &terrainParams) {
    // Cave generation (cheese + spaghetti) using 3D Perlin noise
    // TODO: Spaghetti caves
    const int caveTopY = terrainParams.seaLevel - 20;
    const int yStart   = terrainParams.bedrockLevel + 5;
    Noise cheeseNoise(terrainParams.seed + 7890);

    for (int x = 0; x < Chunk::WIDTH; ++x) {
        const auto worldX = static_cast<float>(originX + x);
        for (int y = yStart; y <= caveTopY; ++y) {
            for (int z = 0; z < Chunk::DEPTH; ++z) {
                const auto worldZ = static_cast<float>(originZ + z);

                float noiseValue = cheeseNoise.getNoise(worldX * 0.1f, y * 0.25f, worldZ * 0.1f);
                if (noiseValue < -0.25f) {
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
            static_cast<unsigned>(terrainParams.seed),
            static_cast<unsigned>(originX),
            static_cast<unsigned>(originZ),
            static_cast<unsigned>(ore.type)
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
    Noise baseNoise(terrainParams.seed);

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
    Noise erosionNoise(terrainParams.seed + 237);

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
    Noise peakValleyNoise(terrainParams.seed + 98789);

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
    Noise tempNoise(terrainParams.seed + 123);

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
    Noise humidNoise(terrainParams.seed + 456);

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

    Noise tempNoise(terrainParams.seed + 45);
    Noise humidNoise(terrainParams.seed + 964);

    const float chunks = glm::max(1, terrainParams.biomeScaleChunks);
    const float worldUnitsPerPatch = chunks * Chunk::WIDTH * 8.0f;
    const float freqCoarse = 1.0f / glm::max(256.0f, worldUnitsPerPatch);

    // Coarse climate fields in [0..1]
    float tempCoarse  = (tempNoise.fractalBrownianMotion2D(worldX * freqCoarse,            worldZ * freqCoarse,            4, 2.0f, 0.5f) + 1.0f) * 0.5f;
    float humidCoarse = (humidNoise.fractalBrownianMotion2D(worldX * freqCoarse * 0.9f,    worldZ * freqCoarse * 0.9f,    4, 2.0f, 0.5f) + 1.0f) * 0.5f;

    // Small regional bias
    Noise regionBias(terrainParams.seed + 4242);
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
