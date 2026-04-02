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

	generateTerrain(blocks, terrainParams);

    generateCaves(blocks, terrainParams);

    generateTrees(blocks, terrainParams);
    generateCacti(blocks, terrainParams);

    generateOres(blocks, terrainParams);
    
    #ifndef NDEBUG
	    stripBlocks(blocks, terrainParams);
    #endif

    // encode palette and block data (must be done before vegetation generation)
    blockIndices.encodeAll(blocks.getData(), palette, paletteMap);

    generateVegetation(blocks, terrainParams);
}

// Core terrain generation: heightmap, biome assignment and base block types.
void ChunkGeneration::generateTerrain(BlockStorage& blocks, const TerrainGenerationParams& terrainParams) {
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
            setBiomeAt(x, z, biome);

            // Set blocks based on biome
            for (int y = std::max(terrainParams.bedrockLevel + 1, surfaceY - 3); y < surfaceY && y < HEIGHT; y++) {
                switch (biome) {
                case BiomeType::DESERT:
                    top = BlockType::SAND;
                    fill = BlockType::SANDSTONE;
                    break;
                case BiomeType::SWAMP:
                    top = BlockType::CLAY;
                    fill = BlockType::COARSE_DIRT;
                    break;
                case BiomeType::TUNDRA:
                    top = BlockType::SNOW;
                    fill = BlockType::DIRT;
                    break;
                case BiomeType::VOLCANIC:
                    top = fill = BlockType::BASALT;
                    break;

                case BiomeType::ICE_PLAINS:
                    top = BlockType::ICE;
                    fill = BlockType::PACKED_ICE;
                    break;

                case BiomeType::MOUNTAIN: {
                    static constexpr BlockType mountainLayers[] = {
                        BlockType::STONE, BlockType::GRANITE, BlockType::STONE,
                        BlockType::DIORITE, BlockType::STONE, BlockType::ANDESITE,
                        BlockType::STONE, BlockType::COBBLESTONE,
                    };
                    constexpr int N = static_cast<int>(std::size(mountainLayers));
                    fill = mountainLayers[((y % N) + N) % N];
                    top = BlockType::STONE;
                    break;
                }

                case BiomeType::RED_DESERT:
                    top = BlockType::RED_SAND;
                    fill = BlockType::RED_SANDSTONE;
                    break;

                case BiomeType::NETHER:
                    top = fill = BlockType::NETHERRACK;
                    break;

                case BiomeType::MUSHROOM_ISLAND:
                    top = BlockType::RED_MUSHROOM_BLOCK;
                    fill = BlockType::COARSE_DIRT;
                    break;
                case BiomeType::MESA: {
                    static constexpr BlockType mesaLayers[] = {
                        BlockType::TERRACOTTA,
                        BlockType::RED_TERRACOTTA,
                        BlockType::ORANGE_TERRACOTTA,
                        BlockType::YELLOW_TERRACOTTA,
                        BlockType::BROWN_TERRACOTTA,
                        BlockType::WHITE_TERRACOTTA,
                        BlockType::RED_TERRACOTTA,
                        BlockType::ORANGE_TERRACOTTA,
                        BlockType::TERRACOTTA,
                        BlockType::PINK_TERRACOTTA,
                    };
                    constexpr int N = static_cast<int>(std::size(mesaLayers));
                    top = fill = mesaLayers[((y % N) + N) % N];
                    break;
                }
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
}


// DEBUG: strip everything except ores so they're visible in isolation
void ChunkGeneration::stripBlocks(BlockStorage& blocks, const TerrainGenerationParams& terrainParams) {
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
}


// Fast integer hash used to create organic leaf edges.
// Returns true ~25% of the time for a given world position + tree anchor.
static inline bool shouldSkipEdgeLeaf(int wx, int wy, int wz, int twx, int twz) {
    uint32_t h = static_cast<uint32_t>(wx * 1619 + wy * 31337 + wz * 6271 + twx * 8191 + twz * 1087);
    h ^= h >> 16;
    h *= 0x45d9f3bu;
    h ^= h >> 16;
    return (h & 0xFF) < 64; // ~25% skip rate
}

// Place a single tree's blocks into the local BlockStorage.
// trunkWorldX/Z is the world-space column of the trunk.
// Only blocks that fall within this chunk's bounds are written.
void ChunkGeneration::placeTree(BlockStorage &blocks, int trunkWorldX, int trunkWorldZ,
                                int surfaceY, int treeHeight,
                                BlockType logType, BlockType leafType, int canopyStyle,
                                int maxTrunkWidth, std::mt19937 &rng) const {

    // Helper: write a log block at world (wx, wz, y), bounds-checked to this chunk.
    auto placeLog = [&](int wx, int wz, int y) {
        const int lx = wx - originX;
        const int lz = wz - originZ;
        if (lx < 0 || lx >= WIDTH || lz < 0 || lz >= DEPTH || y < 0 || y >= HEIGHT) return;
        blocks.at(lx, y, lz) = logType;
    };

    // Helper: fill a horizontal disc of leaves centered at world (cwx, cwz) at height ly.
    // skipCorners=true produces a diamond pattern.
    // organic=true randomly skips ~25% of outermost-ring blocks for a natural edge.
    auto placeLeafLayer = [&](int cwx, int cwz, int ly, int radius, bool skipCorners, bool organic = false) {
        if (ly < 0 || ly >= HEIGHT) return;
        for (int lx = -radius; lx <= radius; ++lx) {
            for (int lz = -radius; lz <= radius; ++lz) {
                if (skipCorners && abs(lx) == radius && abs(lz) == radius)
                    continue;
                if (organic && (abs(lx) == radius || abs(lz) == radius)) {
                    if (shouldSkipEdgeLeaf(cwx + lx, ly, cwz + lz, trunkWorldX, trunkWorldZ)) {
                        // Don't skip if an adjacent block in this layer is a log
                        const int dx[] = {1, -1, 0, 0};
                        const int dz[] = {0, 0, 1, -1};
                        bool nextToLog = false;
                        for (int d = 0; d < 4; ++d) {
                            const int nx = cwx + lx + dx[d] - originX;
                            const int nz = cwz + lz + dz[d] - originZ;
                            if (nx >= 0 && nx < WIDTH && nz >= 0 && nz < DEPTH &&
                                blocks.at(nx, ly, nz) == logType) {
                                nextToLog = true;
                                break;
                            }
                        }
                        if (!nextToLog)
                            continue;
                    }
                }
                const int leafLocalX = cwx + lx - originX;
                const int leafLocalZ = cwz + lz - originZ;
                if (leafLocalX < 0 || leafLocalX >= WIDTH || leafLocalZ < 0 || leafLocalZ >= DEPTH)
                    continue;
                if (blocks.at(leafLocalX, ly, leafLocalZ) != logType)
                    blocks.at(leafLocalX, ly, leafLocalZ) = leafType;
            }
        }
    };

    // --- Compute trunk width (used by trunk placement and canopy) ---
    // Only dark oak, jungle and spruce can have wider trunks (2x2 or 3x3).
    int trunkW = 1;
    if (canopyStyle != 1) { // acacia always 1x1 (builds its own angled trunk)
        if (maxTrunkWidth >= 3 && treeHeight >= 18)
            trunkW = 3;
        else if (maxTrunkWidth >= 2 && treeHeight >= 10)
            trunkW = 2;
    }

    // Canopy center and extra radius to account for wider trunks.
    // For 2x2: center at +1, extraR=1. For 3x3: center at +1, extraR=1.
    const int canopyCX = trunkWorldX + trunkW / 2;
    const int canopyCZ = trunkWorldZ + trunkW / 2;
    const int extraR   = (trunkW > 1) ? 1 : 0;

    // --- Trunk (acacia builds its own angled trunk inside the switch) ---
    if (canopyStyle != 1) {
        if (trunkW > 1) {
            // Dirt under entire base footprint
            for (int ox = 0; ox < trunkW; ++ox)
                for (int oz = 0; oz < trunkW; ++oz) {
                    const int lx = trunkWorldX + ox - originX;
                    const int lz = trunkWorldZ + oz - originZ;
                    if (lx >= 0 && lx < WIDTH && lz >= 0 && lz < DEPTH)
                        blocks.at(lx, surfaceY, lz) = BlockType::DIRT;
                }
            // Full-width trunk goes all the way to the top
            for (int y = surfaceY + 1; y <= surfaceY + treeHeight && y < HEIGHT; ++y)
                for (int ox = 0; ox < trunkW; ++ox)
                    for (int oz = 0; oz < trunkW; ++oz)
                        placeLog(trunkWorldX + ox, trunkWorldZ + oz, y);
        } else {
            const int tlx = trunkWorldX - originX, tlz = trunkWorldZ - originZ;
            if (tlx >= 0 && tlx < WIDTH && tlz >= 0 && tlz < DEPTH) {
                blocks.at(tlx, surfaceY, tlz) = BlockType::DIRT;
                for (int y = surfaceY + 1; y <= surfaceY + treeHeight && y < HEIGHT; ++y)
                    blocks.at(tlx, y, tlz) = logType;
            }
        }
    }

    // --- Branches (Dark Oak height>=7, Jungle height>=10) ---
    const bool doBranches = (canopyStyle == 0 && treeHeight >= 7) ||
                            (canopyStyle == 3 && treeHeight >= 10);
    if (doBranches) {
        const int maxBranches = (canopyStyle == 3) ? 4 : 3;
        const int minBranches = (canopyStyle == 3) ? 2 : 1;
        const int numBranches = minBranches + static_cast<int>(rng() % (maxBranches - minBranches + 1));

        constexpr int bdx[4] = {1, -1, 0,  0};
        constexpr int bdz[4] = {0,  0, 1, -1};

        const int branchMinY = surfaceY + static_cast<int>(treeHeight * 0.55f);
        const int branchMaxY = surfaceY + treeHeight - 2;
        const int branchYRange = std::max(1, branchMaxY - branchMinY);

        for (int b = 0; b < numBranches; ++b) {
            const int dir       = static_cast<int>(rng() % 4);
            const int branchY   = branchMinY + static_cast<int>(rng() % branchYRange);
            const int branchLen = 1 + static_cast<int>(rng() % 2); // 1 or 2

            if (branchY >= HEIGHT) continue;

            // Branch starts from the edge of the trunk
            int curX = canopyCX + bdx[dir] * (trunkW / 2 + 1);
            int curZ = canopyCZ + bdz[dir] * (trunkW / 2 + 1);
            placeLog(curX, curZ, branchY);
            for (int seg = 1; seg < branchLen; ++seg) {
                curX += bdx[dir]; curZ += bdz[dir];
                placeLog(curX, curZ, branchY);
            }
            placeLog(curX, curZ, branchY + 1); // tip angles upward

            const int leafR = branchLen;
            placeLeafLayer(curX, curZ, branchY + 1, leafR,                    true, true);
            placeLeafLayer(curX, curZ, branchY + 2, std::max(1, leafR - 1),   true, true);
        }
    }

    // --- Canopy ---
    switch (canopyStyle) {
        default:
        case 0: {
            // Round pyramid (Oak, Birch, Dark Oak): organic ragged edges
            constexpr int leafRadii[4] = {2, 2, 1, 0};
            for (int layer = 0; layer < 4; ++layer)
                placeLeafLayer(canopyCX, canopyCZ, surfaceY + treeHeight - 2 + layer, leafRadii[layer] + extraR, true, true);
            // Leaf cap above trunk tip
            placeLeafLayer(canopyCX, canopyCZ, surfaceY + treeHeight + 1, extraR, false);
            break;
        }
        case 1: {
            // Acacia: angled main trunk + extra branches that fork off similarly.
            // Branches go mostly sideways: 1 block horizontal shift per 1 block up.
            constexpr int adx[4] = {1, -1,  0, 0};
            constexpr int adz[4] = {0,  0,  1, -1};

            // Dirt under base
            { const int lx = trunkWorldX - originX, lz = trunkWorldZ - originZ;
              if (lx >= 0 && lx < WIDTH && lz >= 0 && lz < DEPTH)
                  blocks.at(lx, surfaceY, lz) = BlockType::DIRT; }

            // Helper: build an angled acacia trunk/branch.
            // Pattern: for each step, place 1 log going sideways, then 1 log going up.
            // This creates a shallow ~45° angle that spreads outward.
            struct BranchTip { int x, z, y; };
            auto buildAcaciaTrunk = [&](int startX, int startZ, int startY,
                                        int numSteps, int leanDir) -> BranchTip {
                const int dx = adx[leanDir], dz = adz[leanDir];
                int curX = startX, curZ = startZ, curY = startY;
                for (int step = 0; step < numSteps && curY + 1 < HEIGHT; ++step) {
                    // Go sideways
                    curX += dx; curZ += dz;
                    // Then go up
                    ++curY;
                    placeLog(curX, curZ, curY);
                }
                return {curX, curZ, curY};
            };

            // Straight vertical trunk base (3-5 blocks tall)
            const int baseHeight = 3 + static_cast<int>(rng() % 3);
            { const int tlx = trunkWorldX - originX, tlz = trunkWorldZ - originZ;
              if (tlx >= 0 && tlx < WIDTH && tlz >= 0 && tlz < DEPTH) {
                  for (int y = surfaceY + 1; y <= surfaceY + baseHeight && y < HEIGHT; ++y)
                      blocks.at(tlx, y, tlz) = logType;
              }
            }

            // Main angled section from top of base
            const int mainLeanDir = static_cast<int>(rng() % 4);
            const int mainSteps   = 3 + static_cast<int>(rng() % 3); // 3..5 steps
            auto mainTip = buildAcaciaTrunk(trunkWorldX, trunkWorldZ,
                                            surfaceY + baseHeight, mainSteps, mainLeanDir);

            // Main canopy
            placeLeafLayer(mainTip.x, mainTip.z, mainTip.y + 2, 3, false);
            placeLeafLayer(mainTip.x, mainTip.z, mainTip.y + 1, 3, false);
            placeLeafLayer(mainTip.x, mainTip.z, mainTip.y,     2, false);
            placeLeafLayer(mainTip.x, mainTip.z, mainTip.y - 1, 2, false);

            // Extra branches forking off the vertical base
            const int numBranches = 1 + static_cast<int>(rng() % 3); // 1..3 branches
            for (int b = 0; b < numBranches; ++b) {
                // Pick a direction different from the main lean
                int branchDir = static_cast<int>(rng() % 4);
                if (branchDir == mainLeanDir)
                    branchDir = (branchDir + 1 + static_cast<int>(rng() % 3)) % 4;

                // Fork from somewhere on the vertical base
                const int forkY    = surfaceY + 2 + static_cast<int>(rng() % std::max(1, baseHeight - 1));
                const int bSteps   = 2 + static_cast<int>(rng() % 3); // 2..4 steps

                auto branchTip = buildAcaciaTrunk(trunkWorldX, trunkWorldZ,
                                                  forkY, bSteps, branchDir);

                // Smaller canopy on branches
                placeLeafLayer(branchTip.x, branchTip.z, branchTip.y + 2, 2, false);
                placeLeafLayer(branchTip.x, branchTip.z, branchTip.y + 1, 2, false);
                placeLeafLayer(branchTip.x, branchTip.z, branchTip.y,     1, false);
            }
            break;
        }
        case 2: {
            // Spruce: pinecone shape, scaled for trunk width.
            const int  N    = std::max(4, std::min(treeHeight - 2, 14));
            const float maxR = ((N >= 12) ? 4.0f : (N >= 8) ? 3.0f : 2.0f) + extraR;

            for (int i = 0; i < N; ++i) {
                int r;
                if (i == 0) {
                    r = 0;
                } else {
                    const float t    = static_cast<float>(i) / (N - 1);
                    float envR;
                    if (t < 0.55f)
                        envR = 1.0f + t / 0.55f * (maxR - 1.0f);
                    else
                        envR = maxR - (t - 0.55f) / 0.45f * (maxR - 1.0f);
                    envR = std::max(1.0f, std::min(maxR, envR));
                    r = (i % 2 == 0) ? static_cast<int>(std::ceil(envR))
                                     : static_cast<int>(std::floor(envR));
                    r = std::max(1, std::min(static_cast<int>(maxR), r));
                }
                placeLeafLayer(canopyCX, canopyCZ, surfaceY + treeHeight - i, r, true);
            }
            // Leaf cap one block above the trunk tip
            placeLeafLayer(canopyCX, canopyCZ, surfaceY + treeHeight + 1, extraR, false);
            break;
        }
        case 3: {
            // Jungle: wide 5-layer lush canopy, organic edges, scaled for trunk width
            const int jungleRadii[] = {1 + extraR, 2 + extraR, 3 + extraR, 3 + extraR, 2 + extraR};
            for (int layer = 0; layer < 5; ++layer)
                placeLeafLayer(canopyCX, canopyCZ, surfaceY + treeHeight - 1 + layer, jungleRadii[layer], false, true);
            // Leaf cap
            placeLeafLayer(canopyCX, canopyCZ, surfaceY + treeHeight + 1, extraR, false);
            break;
        }
    }
}

// Generate trees for this chunk based on biome and deterministic RNG.
void ChunkGeneration::generateTrees(BlockStorage &blocks, const TerrainGenerationParams &terrainParams) const {
    // Tree leaves extend up to 2 blocks horizontally. To handle trees from
    // neighboring chunks whose canopy spills into this chunk, we iterate
    // over the current chunk and all 8 neighbors' tree positions.

    // The maximum horizontal reach of a tree canopy in blocks.
    // Acacia: up to 5 sideways steps + leaf radius 3 = 8. Wide trunks add ~1 extra.
    constexpr int TREE_REACH = 9;

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

            // Only place trees above sea level; leave enough headroom for tallest trees (~27 blocks)
            if (surfaceY <= terrainParams.seaLevel || surfaceY >= HEIGHT - 30) {
                // Still advance the RNG to keep determinism
                rng(); // for the tree chance roll
                continue;
            }

            const BiomeType biome = computeBiome(terrainParams,
                static_cast<float>(worldX), static_cast<float>(worldZ), surfaceY);

            // One chance roll per column — all biomes consume the same number of RNG calls
            // so world generation stays deterministic regardless of which biome a column is in.
            int chanceRoll = static_cast<int>(rng() % 1000);

            // Quick check: can any part of this tree reach into our chunk?
            int localTrunkX = worldX - originX;
            int localTrunkZ = worldZ - originZ;

            int treeHeight;
            switch (biome) {
                case BiomeType::DARK_FOREST:
                    if (chanceRoll >= 10) continue;  // 1.0%
                    treeHeight = 8 + static_cast<int>(rng() % 19); // 8..26, tip up to 27 blocks
                    if (localTrunkX < -TREE_REACH || localTrunkX >= WIDTH + TREE_REACH ||
                        localTrunkZ < -TREE_REACH || localTrunkZ >= DEPTH + TREE_REACH) continue;
                    placeTree(blocks, worldX, worldZ, surfaceY, treeHeight, BlockType::DARK_OAK_LOG, BlockType::DARK_OAK_LEAVES, 0, 3, rng);
                    break;
                case BiomeType::JUNGLE:
                    if (chanceRoll >= 20) continue;  // 2.0%
                    treeHeight = 12 + static_cast<int>(rng() % 13); // 12..24, canopy tip up to 27 blocks
                    if (localTrunkX < -TREE_REACH || localTrunkX >= WIDTH + TREE_REACH ||
                        localTrunkZ < -TREE_REACH || localTrunkZ >= DEPTH + TREE_REACH) continue;
                    placeTree(blocks, worldX, worldZ, surfaceY, treeHeight, BlockType::JUNGLE_LOG, BlockType::JUNGLE_LEAVES, 3, 2, rng);
                    break;
                case BiomeType::SAVANNA:
                    if (chanceRoll >= 6) continue;   // 0.6%
                    treeHeight = 4 + static_cast<int>(rng() % 4);
                    if (localTrunkX < -TREE_REACH || localTrunkX >= WIDTH + TREE_REACH ||
                        localTrunkZ < -TREE_REACH || localTrunkZ >= DEPTH + TREE_REACH) continue;
                    placeTree(blocks, worldX, worldZ, surfaceY, treeHeight, BlockType::ACACIA_LOG, BlockType::ACACIA_LEAVES, 1, 1, rng);
                    break;
                case BiomeType::BIRCH_FOREST:
                    if (chanceRoll >= 12) continue;  // 1.2%
                    treeHeight = 5 + static_cast<int>(rng() % 5);
                    if (localTrunkX < -TREE_REACH || localTrunkX >= WIDTH + TREE_REACH ||
                        localTrunkZ < -TREE_REACH || localTrunkZ >= DEPTH + TREE_REACH) continue;
                    placeTree(blocks, worldX, worldZ, surfaceY, treeHeight, BlockType::BIRCH_LOG, BlockType::BIRCH_LEAVES, 0, 1, rng);
                    break;
                case BiomeType::PLAINS:
                    if (chanceRoll >= 1) continue;          // initial 0.1% filter
                    if (static_cast<int>(rng() % 2) == 0) continue; // halve further → ~0.05%
                    treeHeight = 4 + static_cast<int>(rng() % 5);
                    if (localTrunkX < -TREE_REACH || localTrunkX >= WIDTH + TREE_REACH ||
                        localTrunkZ < -TREE_REACH || localTrunkZ >= DEPTH + TREE_REACH) continue;
                    placeTree(blocks, worldX, worldZ, surfaceY, treeHeight, BlockType::OAK_LOG, BlockType::OAK_LEAVES, 0, 1, rng);
                    break;
                case BiomeType::TUNDRA:
                    if (chanceRoll >= 5) continue;   // 0.5%
                    treeHeight = 8 + static_cast<int>(rng() % 19); // 8..26, tip up to 27 blocks
                    if (localTrunkX < -TREE_REACH || localTrunkX >= WIDTH + TREE_REACH ||
                        localTrunkZ < -TREE_REACH || localTrunkZ >= DEPTH + TREE_REACH) continue;
                    placeTree(blocks, worldX, worldZ, surfaceY, treeHeight, BlockType::SPRUCE_LOG, BlockType::SPRUCE_LEAVES, 2, 3, rng);
                    break;
                default:
                    continue;
            }
        }
    }
}

// Generate cacti in desert biomes. Cacti are 3 blocks tall and can be placed next to each other, but not diagonally.
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

// Carve caves using 3D Perlin noise. Caves are more likely and larger deeper underground, with small narrow entrances near the surface.
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

// Place ore veins based on deterministic RNG.
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

static ClimateTemperature quantizeTemp(float t) {
    // t in [0,1]
    if (t < 0.2f) return ClimateTemperature::VERY_COLD;
    if (t < 0.4f) return ClimateTemperature::COLD;
    if (t < 0.6f) return ClimateTemperature::TEMPERATE;
    if (t < 0.8f) return ClimateTemperature::WARM;
    return ClimateTemperature::HOT;
}

static ClimateHumidity quantizeHumidity(float h) {
    // h in [0,1]
    if (h < 0.2f) return ClimateHumidity::ARID;
    if (h < 0.4f) return ClimateHumidity::DRY;
    if (h < 0.6f) return ClimateHumidity::NEUTRAL;
    if (h < 0.8f) return ClimateHumidity::HUMID;
    return ClimateHumidity::WET;
}

static ClimateErosion quantizeErosion(float e) {
    // e in [-1,1]
    if (e < -0.71f) return ClimateErosion::E0;
    if (e < -0.43f) return ClimateErosion::E1;
    if (e < -0.14f) return ClimateErosion::E2;
    if (e < 0.14f) return ClimateErosion::E3;
    if (e < 0.43f) return ClimateErosion::E4;
    if (e < 0.71f) return ClimateErosion::E5;
    return ClimateErosion::E6;
}

static ClimateContinentalness quantizeContinentalness(float c) {
    // c in [-3.8,3.8]
    if (c < -1.05f) return ClimateContinentalness::MUSHROOM;
    if (c < -0.455f) return ClimateContinentalness::OCEAN;
    if (c < -0.15f) return ClimateContinentalness::COAST;
    if (c < 0.165f) return ClimateContinentalness::NEAR_INLAND;
    if (c < 0.73f) return ClimateContinentalness::MID_INLAND;
    return ClimateContinentalness::FAR_INLAND;
}

static ClimatePeaksValleys quantizePV(float pv) {
    // pv in [-1,1]
    if (pv < -0.6f) return ClimatePeaksValleys::VALLEY;
    if (pv < -0.2f) return ClimatePeaksValleys::LOW;
    if (pv < 0.2f) return ClimatePeaksValleys::MID;
    if (pv < 0.6f) return ClimatePeaksValleys::HIGH;
    return ClimatePeaksValleys::PEAK;
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
    // Perlin's actual output range is ~[-0.5, 0.5] (unit gradient dot sub-cell distance),
    // so multiply by 2 before normalizing to stretch the full [0,1] range and reach
    // extreme buckets (VERY_COLD, HOT) that produce Tundra, Mesa, and Jungle.
    float tempCoarse  = glm::clamp((tempNoise.fractalBrownianMotion2D(worldX * freqCoarse,         worldZ * freqCoarse,         4, 2.0f, 0.5f) * 2.0f + 1.0f) * 0.5f, 0.0f, 1.0f);
    float humidCoarse = glm::clamp((humidNoise.fractalBrownianMotion2D(worldX * freqCoarse * 0.9f, worldZ * freqCoarse * 0.9f, 4, 2.0f, 0.5f) * 2.0f + 1.0f) * 0.5f, 0.0f, 1.0f);


    const float rawCont = getContinentalness(terrainParams, worldX, worldZ);
    const float rawEro = getErosion(terrainParams, worldX, worldZ);
    const float rawPV = getPV(terrainParams, worldX, worldZ);

    const auto ct = quantizeTemp(tempCoarse);
    const auto ch = quantizeHumidity(humidCoarse);
    const auto ce = quantizeErosion(rawEro);
    const auto cc = quantizeContinentalness(rawCont);
    const auto cpv = quantizePV(rawPV);

    // OCEAN
    // if (cc == ClimateContinentalness::OCEAN) return BiomeType::OCEAN;

    // Mushroom Island — cold, wet, near coast/ocean (old continentalness check was almost
    // always filtered out by the seaLevel height check since MUSHROOM continental = underwater)
    if (ct == ClimateTemperature::COLD &&
        ch == ClimateHumidity::WET &&
        cc <= ClimateContinentalness::NEAR_INLAND)
        return BiomeType::MUSHROOM_ISLAND;

    // VERY COLD
    if (ct == ClimateTemperature::VERY_COLD) {
        return (ch >= ClimateHumidity::NEUTRAL) ? BiomeType::ICE_PLAINS : BiomeType::TUNDRA;
    }

    // Nether — hot, arid, elevated terrain (high or peak peaks)
    if (ct == ClimateTemperature::HOT &&
        ch == ClimateHumidity::ARID &&
        cpv >= ClimatePeaksValleys::HIGH)
        return BiomeType::NETHER;

    // Volcanic — hot, arid, flat-to-mid terrain (elevated Nether already claimed above)
    if (ct == ClimateTemperature::HOT &&
        ch == ClimateHumidity::ARID)
        return BiomeType::VOLCANIC;

    // Canyon terrain is now part of Mesa (same biome, erosion gives it canyons naturally)
    // Mesa — hot, dry/arid-but-not-arid (ARID is claimed by VOLCANIC above), or
    //        warm/hot + dry + heavily eroded (canyon-like mesa)
    if ((ct == ClimateTemperature::WARM || ct == ClimateTemperature::HOT) &&
        ch <= ClimateHumidity::DRY &&
        ce >= ClimateErosion::E4 &&
        cc >= ClimateContinentalness::NEAR_INLAND)
        return BiomeType::MESA;

    if (ct == ClimateTemperature::HOT && ch == ClimateHumidity::DRY)
        return BiomeType::MESA;

    // Red Desert — warm + arid flat lands (distinct from hot MESA)
    if (ct == ClimateTemperature::WARM && ch == ClimateHumidity::ARID)
        return BiomeType::RED_DESERT;

    // Desert — warm/hot, arid/dry only (NEUTRAL excluded so Savanna can claim WARM+NEUTRAL)
    if ((ct == ClimateTemperature::WARM || ct == ClimateTemperature::HOT) &&
        ch <= ClimateHumidity::DRY && height < 110)
        return BiomeType::DESERT;

    // Savanna — warm, moderate (checked before Jungle/Swamp to avoid being swallowed)
    if (ct == ClimateTemperature::WARM && ch == ClimateHumidity::NEUTRAL)
        return BiomeType::SAVANNA;

    // Jungle — hot and wet
    if (ct == ClimateTemperature::HOT && ch >= ClimateHumidity::HUMID)
        return BiomeType::JUNGLE;

    // Swamp — warm, wet, flat/valley
    if ((ct == ClimateTemperature::TEMPERATE || ct == ClimateTemperature::WARM) &&
        ch >= ClimateHumidity::HUMID &&
        cpv <= ClimatePeaksValleys::LOW)
        return BiomeType::SWAMP;

    // Dark Forest — cool/temperate, humid
    if (ct <= ClimateTemperature::TEMPERATE && ch >= ClimateHumidity::HUMID)
        return BiomeType::DARK_FOREST;

    // Mountain — high peaks, inland (checked before Birch Forest so mountainous
    // temperate+neutral terrain becomes mountains, not forest)
    if (cpv >= ClimatePeaksValleys::HIGH && cc >= ClimateContinentalness::MID_INLAND)
        return BiomeType::MOUNTAIN;

    // Birch Forest — temperate, neutral humidity, low/mid terrain
    if (ct == ClimateTemperature::TEMPERATE &&
        (ch == ClimateHumidity::NEUTRAL || ch == ClimateHumidity::HUMID))
        return BiomeType::BIRCH_FOREST;





    // Small regional bias
    // static Noise regionBias(terrainParams.seed + 4242);
    // float bias = (regionBias.fractalBrownianMotion2D(worldX * freqCoarse * 0.6f, worldZ * freqCoarse * 0.6f, 3, 2.0f, 0.5f) + 1.0f) * 0.5f;

    // float climate = glm::clamp(glm::mix(tempCoarse, 1.0f - humidCoarse, 0.35f) * 0.7f + bias * 0.3f, 0.0f, 1.0f);


    // climate = glm::clamp((climate - 0.5f) * 1.2f + 0.5f, 0.0f, 1.0f);

    // // High, cold overrides
    // // if (height > terrainParams.seaLevel + 28) {
    //     if (tempCoarse < terrainParams.snowTemperatureThreshold) return BiomeType::TUNDRA;
    //     // return BiomeType::MOUNTAIN;
    // // }
    // float pv = getPV(terrainParams, worldX, worldZ);
    // float aridity = (1.0f - humidCoarse) * tempCoarse;

    // // MESA: hot + very dry — terracotta terrain
    // if (tempCoarse > 0.60f && humidCoarse < 0.35f && aridity > 0.25f)
    //     return BiomeType::MESA;

    // // --- DESERT: hot + dry, inland, mid elevations ---
    // if (aridity > 0.3f &&
    //     tempCoarse > 0.40f &&
    //     humidCoarse < 0.45f &&
    //     height <= 90 && pv < 0.2f
    //     ){
    //     return BiomeType::DESERT;
    //     }

    // // Cold lowlands
    // if (climate < 0.16f) return BiomeType::TUNDRA;
    // if (height > terrainParams.seaLevel + 30 && tempCoarse < 0.45f) return BiomeType::TUNDRA;
    
    // if (tempCoarse > 0.6f && humidCoarse > 0.6f) return BiomeType::JUNGLE;

    // // SWAMP: wet, low-lying, mild temps
    // if (height <= terrainParams.seaLevel + 6 &&
    //     humidCoarse > 0.60f &&
    //     tempCoarse > 0.30f && tempCoarse < 0.80f) {
    //     return BiomeType::SWAMP;
    // }

    // if (tempCoarse > 0.35f && tempCoarse < 0.55f && humidCoarse > 0.50f) return BiomeType::BIRCH_FOREST;

    // // Forest: moist and not too hot
    // if (humidCoarse > terrainParams.forestMoistureThreshold * 0.9f && climate < 0.65f) return BiomeType::DARK_FOREST;


    // if (tempCoarse > 0.55f && humidCoarse > 0.35f && humidCoarse < 0.55f) return BiomeType::SAVANNA;



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

// Place grass and flowers based on deterministic RNG and biome-specific spawn chances, weighted tables, and placement rules.
void ChunkGeneration::generateVegetation(const BlockStorage &blocks, const TerrainGenerationParams &terrainParams) {

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

            const BiomeType biome = getBiomeAt(x, z);
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

                // Only place vegetation on grass, dirt, sand, or terracotta (mesa)
                if (surfaceBlock != BlockType::GRASS && surfaceBlock != BlockType::DIRT &&
                    surfaceBlock != BlockType::SAND && biome != BiomeType::MESA)
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
                case BiomeType::PLAINS:       spawnChance = 10; break;
                case BiomeType::DARK_FOREST:  spawnChance = 5;  break;
                case BiomeType::JUNGLE:       spawnChance = 15; break;
                case BiomeType::SAVANNA:      spawnChance = 4;  break;
                case BiomeType::BIRCH_FOREST: spawnChance = 8;  break;
                case BiomeType::SWAMP:        spawnChance = 0;  break;
                case BiomeType::OCEAN:        spawnChance = 20; break;
                case BiomeType::DESERT:       spawnChance = 1;  break;
                case BiomeType::MESA:         spawnChance = 1;  break;
                case BiomeType::TUNDRA:       spawnChance = 0;  break;
                default:                      spawnChance = 0;  break;
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
                { BlockType::DEAD_BUSH, 100 },
            };

            static constexpr VegEntry jungleVeg[] = {
                { BlockType::SHORT_GRASS,        80 },
                { BlockType::BLUE_ORCHID,        10 },
                { BlockType::ALLIUM,              5 },
                { BlockType::CORNFLOWER,          3 },
                { BlockType::POPPY,               2 },
            };

            static constexpr VegEntry savannaVeg[] = {
                { BlockType::SHORT_GRASS, 60 },
                { BlockType::DEAD_BUSH,   40 },
            };

            static constexpr VegEntry birchVeg[] = {
                { BlockType::SHORT_GRASS,        60 },
                { BlockType::POPPY,               5 },
                { BlockType::CORNFLOWER,          5 },
                { BlockType::DANDELION,           5 },
                { BlockType::AZURE_BLUET,         4 },
                { BlockType::OXEYE_DAISY,         3 },
                { BlockType::LILY_OF_THE_VALLEY,  2 },
            };

            static constexpr VegEntry mesaVeg[] = {
                { BlockType::DEAD_BUSH, 100 },
            };

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
                case BiomeType::DARK_FOREST:
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
                case BiomeType::JUNGLE:
                    vegTable = jungleVeg;
                    vegTableSize = std::size(jungleVeg);
                    break;
                case BiomeType::SAVANNA:
                    vegTable = savannaVeg;
                    vegTableSize = std::size(savannaVeg);
                    break;
                case BiomeType::BIRCH_FOREST:
                    vegTable = birchVeg;
                    vegTableSize = std::size(birchVeg);
                    break;
                case BiomeType::MESA:
                    vegTable = mesaVeg;
                    vegTableSize = std::size(mesaVeg);
                    break;
                default:
                    continue; // No vegetation in tundra, mountain, swamp
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
                if (!isStackableSeaVegetation(vegType)) {
                    // Single-block plant: seagrass or coral
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
                // Land vegetation: stored only in the block grid.
                // buildVegetationMesh() scans blocks to derive instances for rendering.
                setBlock(x, surfaceY + 1, z, vegType);
            }
        }
    }
}
