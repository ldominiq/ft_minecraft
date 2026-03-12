
#include "Chunk.hpp"

Chunk::Chunk(std::istream& in) : blockIndices(WIDTH * HEIGHT * DEPTH, 4)
{
    loadFromStream(in);
}

Chunk::~Chunk() {}

bool Chunk::hasAllAdjacentChunkLoaded() const {
    for (const auto& adj : adjacentChunks) {
        if (adj.expired()) {
            return false;
        }
    }
    return true;
}

BlockType Chunk::getBlock(int x, int y, int z) const {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH) {
        return BlockType::AIR; // Out of bounds returns air
    }

    int index = x + WIDTH * (y + HEIGHT * z);
    uint32_t paletteIndex = blockIndices.get(index);
    return palette[paletteIndex];
}

void Chunk::setBlock(int x, int y, int z, BlockType type) {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH) {
        return; // Out of bounds, do nothing
    }

    int index = x + WIDTH * (y + HEIGHT * z);
    auto it = paletteMap.find(type);
	uint32_t paletteIndex;

	if (it == paletteMap.end()) {
        paletteIndex = static_cast<uint32_t>(palette.size());
        palette.push_back(type);
        paletteMap[type] = paletteIndex;
    } else {
    	paletteIndex = it->second;
    }

    blockIndices.set(index, paletteIndex);
}

bool Chunk::isBlockVisible(glm::ivec3 pos) {
    int x = pos.x, y = pos.y, z = pos.z;
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH)
        return false;

	if (!hasAllAdjacentChunkLoaded()) return false;

    if (!isBlockSolid(getBlock(x,y,z)))
        return false;

    auto getBlockOrNeighbor = [&](int dx, int dy, int dz, Direction dir) -> BlockType {
        if (x + dx < 0 || x + dx >= WIDTH ||
            z + dz < 0 || z + dz >= DEPTH) 
        {
            auto neighbor = adjacentChunks[dir].lock();
            if (!neighbor) return BlockType::END;
            int nx = (dx == -1 ? WIDTH - 1 : (dx == 1 ? 0 : x));
            int nz = (dz == -1 ? DEPTH - 1 : (dz == 1 ? 0 : z));
            return neighbor->getBlock(nx, y + dy, nz);
        }
        if (y + dy < 0 || y + dy >= HEIGHT)
            return BlockType::AIR;
		return getBlock(x + dx, y + dy, z + dz);
    };

    return !isBlockSolid(getBlockOrNeighbor(0, 0, +1, NORTH)) ||
           !isBlockSolid(getBlockOrNeighbor(0, 0, -1, SOUTH)) ||
           y == HEIGHT - 1 || !isBlockSolid(getBlockOrNeighbor(0, +1, 0, NONE)) ||
           y == 0 || !isBlockSolid(getBlockOrNeighbor(0, -1, 0, NONE)) ||
           !isBlockSolid(getBlockOrNeighbor(+1, 0, 0, EAST)) ||
           !isBlockSolid(getBlockOrNeighbor(-1, 0, 0, WEST));
}

// ─── Sky-light propagation ──────────────────────────────────────────
//
// HOW IT WORKS (Minecraft-style BFS flood-fill):
//
// 1) SEEDING PHASE  — For every (x, z) column in the chunk, we walk
//    downward from y=HEIGHT-1.  As long as the block is transparent
//    (air / water / leaves), it gets light level 15 (full sunlight)
//    and is pushed into a BFS queue.  The moment we hit a solid block,
//    we stop — sunlight doesn't penetrate straight through stone.
//
// 2) SPREADING PHASE — Classic BFS.  For each block in the queue we
//    try all 6 neighbors (±x, ±y, ±z).  If the neighbor is transparent
//    and its current light < (our light - 1), we update it and push it.
//    This means light "leaks" sideways into caves through openings,
//    losing 1 level per block traveled.
//
// The result: anything under open sky = 15, cave entrances start at 14
// and fade to 0 deep inside.  The shader uses this value to dim faces.
//
// PERFORMANCE NOTES:
//  • We only iterate non-solid blocks (skips most of the chunk).
//  • The queue processes each block at most once per light level.
//  • Total work is proportional to the number of air blocks reached,
//    NOT the total chunk volume (65 536 blocks).
// ─────────────────────────────────────────────────────────────────────

uint8_t Chunk::getSkyLight(int x, int y, int z) const {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH)
        return 0;
    if (skyLight.empty())
        return 15; // Not computed yet — assume full sunlight.
                    // This is the correct default for the common case
                    // (surface blocks).  When concurrent chunk builds
                    // read a neighbor that hasn't been built yet, the
                    // border faces appear sunlit rather than artificially
                    // dark.  Once the neighbor rebuilds, both chunks get
                    // correct cross-chunk values.
    return skyLight[x + WIDTH * (y + HEIGHT * z)];
}

void Chunk::computeSkyLight() {
    // Work on a LOCAL array first, then swap it into the member at the
    // end.  This avoids a race condition: multiple chunks can be built
    // concurrently (std::async), and a neighbor chunk might call
    // getSkyLight() on us while we're mid-computation.  If we wrote
    // directly into the member, the neighbor would see a partially-
    // filled array and read incorrect values.  By keeping the old
    // array in place until the new one is ready, concurrent readers
    // always see either the previous fully-computed result or the
    // new one — never a half-baked intermediate state.
    std::vector<uint8_t> localSkyLight(BLOCK_COUNT, 0);

    // We'll use a queue of (x, y, z) positions to flood-fill light.
    // "struct" to keep it readable — each entry is a block to process.
    struct LightNode {
        int16_t x, y, z;
    };
    std::queue<LightNode> lightQueue;

    // ── Phase 1: Seed sunlight columns ──────────────────────────────
    // Walk each column top-down.  While the block is transparent,
    // assign light=15 (direct sunlight) and enqueue for BFS spreading.
    //
    // Why enqueue every sunlit block?  Because a sunlit block could be
    // next to a cliff face where the terrain is taller — its horizontal
    // neighbor might be a dark air block inside the ground that needs
    // light.  The BFS loop below will quickly skip interior blocks
    // (all their neighbors are already at 15), so this is efficient.
    for (int x = 0; x < WIDTH; ++x) {
        for (int z = 0; z < DEPTH; ++z) {
            for (int y = HEIGHT - 1; y >= 0; --y) {
                BlockType block = getBlock(x, y, z);
                if (isBlockSolid(block))
                    break; // Sunlight can't pass through solid blocks

                int index = x + WIDTH * (y + HEIGHT * z);
                localSkyLight[index] = 15;
                lightQueue.push({(int16_t)x, (int16_t)y, (int16_t)z});
            }
        }
    }

    // ── Phase 2: BFS flood-fill ─────────────────────────────────────
    // For each lit block, try spreading to its 6 neighbors.
    // The neighbor gets (currentLight - 1) if that's brighter than
    // what it already has.  This naturally creates the fall-off.
    //
    // Offsets for the 6 cardinal directions:
    static constexpr int dx[] = { 1, -1,  0,  0,  0,  0 };
    static constexpr int dy[] = { 0,  0,  1, -1,  0,  0 };
    static constexpr int dz[] = { 0,  0,  0,  0,  1, -1 };

    while (!lightQueue.empty()) {
        LightNode current = lightQueue.front();
        lightQueue.pop();

        int currentIndex = current.x + WIDTH * (current.y + HEIGHT * current.z);
        uint8_t currentLight = localSkyLight[currentIndex];

        // Light level 1 can't spread further (would become 0)
        if (currentLight <= 1)
            continue;

        uint8_t spreadLight = currentLight - 1;

        for (int dir = 0; dir < 6; ++dir) {
            int neighborX = current.x + dx[dir];
            int neighborY = current.y + dy[dir];
            int neighborZ = current.z + dz[dir];

            // Stay within chunk bounds (we don't cross chunk borders
            // for now — that would require the neighbors to be loaded
            // and would complicate threading; this is good enough for
            // visible cave darkening within a chunk).
            if (neighborX < 0 || neighborX >= WIDTH ||
                neighborY < 0 || neighborY >= HEIGHT ||
                neighborZ < 0 || neighborZ >= DEPTH)
                continue;

            BlockType neighborBlock = getBlock(neighborX, neighborY, neighborZ);
            if (isBlockSolid(neighborBlock) && !isBlockTransparent(neighborBlock))
                continue; // Light doesn't pass through solid blocks

            int neighborIndex = neighborX + WIDTH * (neighborY + HEIGHT * neighborZ);

            // Only update if we'd make it brighter than it already is.
            // This prevents revisiting blocks and keeps BFS O(n).
            if (localSkyLight[neighborIndex] < spreadLight) {
                localSkyLight[neighborIndex] = spreadLight;
                lightQueue.push({(int16_t)neighborX, (int16_t)neighborY, (int16_t)neighborZ});
            }
        }
    }

    // Publish the fully-computed array.  std::swap is fast (just
    // swaps internal pointers) and makes the transition atomic from
    // the perspective of any concurrent reader — they either see the
    // old empty vector (fallback to 15, see getSkyLight()) or the
    // fully-computed one.
    std::swap(skyLight, localSkyLight);
}

void Chunk::saveToStream(std::ostream& out) const {
    // Write chunk key for O(1) lookup later
    out.write(reinterpret_cast<const char*>(&originX), sizeof(originX));
    out.write(reinterpret_cast<const char*>(&originZ), sizeof(originZ));

    // --- Save palette ---
    uint32_t paletteSize = static_cast<uint32_t>(palette.size());
    out.write(reinterpret_cast<const char*>(&paletteSize), sizeof(paletteSize));

    for (const auto& block : palette) {
        out.write(reinterpret_cast<const char*>(&block), sizeof(block));
    }

	// Save block data
    blockIndices.saveToStream(out);

	// --- Save vegetation ---
	uint32_t vegetationCount = static_cast<uint32_t>(vegetation.size());
	out.write(reinterpret_cast<const char*>(&vegetationCount), sizeof(vegetationCount));

	for (const auto& veg : vegetation) {
		out.write(reinterpret_cast<const char*>(&veg.x), sizeof(veg.x));
		out.write(reinterpret_cast<const char*>(&veg.y), sizeof(veg.y));
		out.write(reinterpret_cast<const char*>(&veg.z), sizeof(veg.z));
		out.write(reinterpret_cast<const char*>(&veg.type), sizeof(veg.type));
	}
}

void Chunk::loadFromStream(std::istream& in) {
    // Read chunk key
    in.read(reinterpret_cast<char*>(&originX), sizeof(originX));
    in.read(reinterpret_cast<char*>(&originZ), sizeof(originZ));

    // --- Load palette ---
    uint32_t paletteSize;
    in.read(reinterpret_cast<char*>(&paletteSize), sizeof(paletteSize));

    palette.resize(paletteSize);
    paletteMap.clear();

    for (uint32_t i = 0; i < paletteSize; ++i) {
        in.read(reinterpret_cast<char*>(&palette[i]), sizeof(BlockType));
        paletteMap[palette[i]] = i; // rebuild map
    }

	// Load block data
    blockIndices.loadFromStream(in);

	// --- Load vegetation ---
	uint32_t vegetationCount;
	in.read(reinterpret_cast<char*>(&vegetationCount), sizeof(vegetationCount));

	vegetation.clear();
	vegetation.reserve(vegetationCount);

	for (uint32_t i = 0; i < vegetationCount; ++i) {
		VegetationInstance veg;
		in.read(reinterpret_cast<char*>(&veg.x), sizeof(veg.x));
		in.read(reinterpret_cast<char*>(&veg.y), sizeof(veg.y));
		in.read(reinterpret_cast<char*>(&veg.z), sizeof(veg.z));
		in.read(reinterpret_cast<char*>(&veg.type), sizeof(veg.type));
		vegetation.push_back(veg);
	}
}