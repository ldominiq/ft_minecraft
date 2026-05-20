
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
        if (paletteIndex >= (1u << blockIndices.bitsPerEntry())) {
            uint8_t needed = 1;
            while ((1u << needed) <= paletteIndex) ++needed;
            blockIndices.grow(needed);
        }
    } else {
    	paletteIndex = it->second;
    }

    blockIndices.set(index, paletteIndex);

    if (isTorch(type))
        containsTorch = true; // cleared/refreshed authoritatively in computeBlockLight()
}

bool Chunk::setBlockCascade(int x, int y, int z, BlockType type) {
    bool clearedVeg = false;
    if (type == BlockType::AIR && y + 1 < HEIGHT) {
        BlockType above = getBlock(x, y + 1, z);
        if (isBlockVegetation(above)) {
            setBlock(x, y + 1, z, BlockType::AIR);
            clearedVeg = true;
        }
    }
    setBlock(x, y, z, type);
    return clearedVeg;
}

bool Chunk::isBlockVisible(glm::ivec3 pos) {
    int x = pos.x, y = pos.y, z = pos.z;
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH)
        return false;

	if (!hasAllAdjacentChunkLoaded()) return false;

    BlockType block = getBlock(x, y, z);
    if (isBlockVegetation(block))
        return true;
    if (!isBlockSolid(block))
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
                if (isBlockSolid(block) && !isBlockTransparent(block))
                    break; // Sunlight can't pass through solid opaque blocks

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

uint8_t Chunk::getBlockLight(int x, int y, int z) const {
    if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT || z < 0 || z >= DEPTH)
        return 0;
    if (blockLight.empty())
        return 0; // Not computed yet / no emitters — fully dark.
    return blockLight[x + WIDTH * (y + HEIGHT * z)];
}

// Block-light propagation. Each chunk computes its own light INDEPENDENTLY
// from the actual torch blocks within reach (its own + the surrounding
// chunks, diagonals included), by running the BFS over a region expanded by
// MARGIN on the X/Z sides. Because it depends only on torch *blocks* (never
// on a neighbour's computed light) there is no cross-chunk feedback: placing
// lights correctly across seams/diagonals, and breaking clears immediately.
// The expensive cross-chunk scan is skipped entirely when no torch is in or
// next to this chunk (the common case — generated terrain has none).
void Chunk::computeBlockLight() {
    static constexpr int MARGIN = 14;          // max torch travel (14 → 0)
    static constexpr int EW = WIDTH + 2 * MARGIN;
    static constexpr int ED = DEPTH + 2 * MARGIN;

    std::vector<uint8_t> localBlockLight(BLOCK_COUNT, 0);

    // Resolve a block at coords that may extend MARGIN outside this chunk,
    // chaining at most one X and one Z hop through adjacentChunks (MARGIN <
    // WIDTH so a coordinate never crosses more than one border per axis).
    auto resolve = [&](int gx, int gy, int gz) -> BlockType {
        if (gy < 0 || gy >= HEIGHT) return BlockType::AIR;
        int lx = gx, lz = gz, sx = 0, sz = 0;
        if (lx < 0)        { sx = -1; lx += WIDTH; }
        else if (lx >= WIDTH) { sx = 1; lx -= WIDTH; }
        if (lz < 0)        { sz = -1; lz += DEPTH; }
        else if (lz >= DEPTH) { sz = 1; lz -= DEPTH; }
        if (sx == 0 && sz == 0) return getBlock(lx, gy, lz);
        Chunk* cur = this;
        std::shared_ptr<Chunk> hold;
        if (sx != 0) {
            hold = adjacentChunks[sx == 1 ? EAST : WEST].lock();
            if (!hold) return BlockType::AIR;
            cur = hold.get();
        }
        if (sz != 0) {
            std::shared_ptr<Chunk> hold2 =
                cur->adjacentChunks[sz == 1 ? NORTH : SOUTH].lock();
            if (!hold2) return BlockType::AIR;
            return hold2->getBlock(lx, gy, lz);
        }
        return cur->getBlock(lx, gy, lz);
    };

    struct LightNode { int16_t x, y, z; };
    std::queue<LightNode> lightQueue;

    // ── Own torches (cheap, also refreshes containsTorch authoritatively) ──
    std::vector<uint8_t> expanded; // allocated only if cross-chunk needed
    bool foundOwn = false;
    for (int x = 0; x < WIDTH && !foundOwn; ++x)
        for (int z = 0; z < DEPTH && !foundOwn; ++z)
            for (int y = 0; y < HEIGHT; ++y)
                if (isTorch(getBlock(x, y, z))) { foundOwn = true; break; }
    containsTorch = foundOwn;

    // The 9 contributing chunks (self + 8 around), locked ONCE here and held
    // alive for the whole pass. Diagonals chain one X then one Z hop, exactly
    // like resolve(). chunkAt(0,0) is *this* (returned as null; handled below).
    auto chunkAt = [&](int cx, int cz) -> std::shared_ptr<Chunk> {
        if (cx != 0) {
            auto a = adjacentChunks[cx == 1 ? EAST : WEST].lock();
            if (!a || cz == 0) return a;
            return a->adjacentChunks[cz == 1 ? NORTH : SOUTH].lock();
        }
        if (cz != 0) return adjacentChunks[cz == 1 ? NORTH : SOUTH].lock();
        return nullptr;
    };
    std::shared_ptr<Chunk> held[3][3];
    bool neighborTorch = false;
    for (int cx = -1; cx <= 1; ++cx)
        for (int cz = -1; cz <= 1; ++cz) {
            if (cx == 0 && cz == 0) continue;
            held[cx + 1][cz + 1] = chunkAt(cx, cz);
            if (auto* c = held[cx + 1][cz + 1].get(); c && c->containsTorch)
                neighborTorch = true;
        }

    if (!containsTorch && !neighborTorch) {
        std::swap(blockLight, localBlockLight); // nothing emits — all dark
        return;
    }

    // Cross-chunk path. Seeds are collected by scanning ONLY the chunks that
    // actually contain a torch (containsTorch) in their own local coords — no
    // per-cell weak_ptr locking. Empty neighbours (the vast majority) cost
    // nothing. We also track the torch Y-range so the BFS volume can be
    // clamped to [minY-MARGIN, maxY+MARGIN] instead of the full 256 columns:
    // light can't travel more than MARGIN, so everything outside that band is
    // 0 anyway. A surface torch shrinks the volume/zero-fill/BFS/copy ~9x.
    const int gxLo = -MARGIN, gxHi = WIDTH - 1 + MARGIN;
    const int gzLo = -MARGIN, gzHi = DEPTH - 1 + MARGIN;

    std::vector<LightNode> seeds;
    int gyMin = HEIGHT, gyMax = -1;
    auto seedChunk = [&](Chunk* c, int baseGx, int baseGz) {
        if (!c || !c->containsTorch) return;
        for (int lx = 0; lx < WIDTH; ++lx) {
            const int gx = baseGx + lx;
            if (gx < gxLo || gx > gxHi) continue;
            for (int lz = 0; lz < DEPTH; ++lz) {
                const int gz = baseGz + lz;
                if (gz < gzLo || gz > gzHi) continue;
                for (int gy = 0; gy < HEIGHT; ++gy)
                    if (isTorch(c->getBlock(lx, gy, lz))) {
                        seeds.push_back({(int16_t)gx, (int16_t)gy, (int16_t)gz});
                        gyMin = std::min(gyMin, gy);
                        gyMax = std::max(gyMax, gy);
                    }
            }
        }
    };
    seedChunk(this, 0, 0);
    for (int cx = -1; cx <= 1; ++cx)
        for (int cz = -1; cz <= 1; ++cz) {
            if (cx == 0 && cz == 0) continue;
            seedChunk(held[cx + 1][cz + 1].get(), cx * WIDTH, cz * DEPTH);
        }

    if (seeds.empty()) {
        std::swap(blockLight, localBlockLight); // stale flags, nothing emits
        return;
    }

    // Y band the BFS is allowed to touch (clamped to the world).
    const int gyLo = std::max(0, gyMin - MARGIN);
    const int gyHi = std::min(HEIGHT - 1, gyMax + MARGIN);
    const int EH   = gyHi - gyLo + 1;
    auto eIdx = [=](int gx, int gy, int gz) -> int {
        return (gx + MARGIN) + EW * ((gy - gyLo) + EH * (gz + MARGIN));
    };

    expanded.assign((size_t)EW * EH * ED, 0);
    for (const LightNode& s : seeds) {
        expanded[eIdx(s.x, s.y, s.z)] = 14;
        lightQueue.push(s);
    }

    static constexpr int dx[] = { 1, -1,  0,  0,  0,  0 };
    static constexpr int dy[] = { 0,  0,  1, -1,  0,  0 };
    static constexpr int dz[] = { 0,  0,  0,  0,  1, -1 };
    while (!lightQueue.empty()) {
        LightNode c = lightQueue.front();
        lightQueue.pop();
        uint8_t cur = expanded[eIdx(c.x, c.y, c.z)];
        if (cur <= 1) continue;
        uint8_t spread = cur - 1;
        for (int dir = 0; dir < 6; ++dir) {
            int nx = c.x + dx[dir], ny = c.y + dy[dir], nz = c.z + dz[dir];
            if (nx < gxLo || nx > gxHi || ny < gyLo || ny > gyHi ||
                nz < gzLo || nz > gzHi)
                continue;
            BlockType nb = resolve(nx, ny, nz);
            if (isBlockSolid(nb) && !isBlockTransparent(nb)) continue;
            int ni = eIdx(nx, ny, nz);
            if (expanded[ni] < spread) {
                expanded[ni] = spread;
                lightQueue.push({(int16_t)nx, (int16_t)ny, (int16_t)nz});
            }
        }
    }

    // Copy the central (this-chunk) region out. Rows outside the lit band
    // stay 0 (localBlockLight is zero-initialised).
    for (int x = 0; x < WIDTH; ++x)
        for (int z = 0; z < DEPTH; ++z)
            for (int y = gyLo; y <= gyHi; ++y)
                localBlockLight[x + WIDTH * (y + HEIGHT * z)] =
                    expanded[eIdx(x, y, z)];

    std::swap(blockLight, localBlockLight);
}

namespace {
    constexpr int LDX[6] = { 1, -1,  0,  0,  0,  0 };
    constexpr int LDY[6] = { 0,  0,  1, -1,  0,  0 };
    constexpr int LDZ[6] = { 0,  0,  0,  0,  1, -1 };
    // Mirrors computeBlockLight's occlusion rule: a torch (non-solid) and
    // transparent blocks let light pass; opaque solids stop it.
    inline bool lightPasses(BlockType b) {
        return !(isBlockSolid(b) && !isBlockTransparent(b));
    }
}

// Shared cross-chunk context for the two incremental ops
struct Chunk::IncLightCtx {
    Chunk* g[3][3] = {};
    std::shared_ptr<Chunk> hold[3][3];
    bool (*touched)[3];

    IncLightCtx(Chunk* self, bool t[3][3]) : touched(t) {
        g[1][1] = self;
        hold[2][1] = self->adjacentChunks[EAST].lock();  g[2][1] = hold[2][1].get();
        hold[0][1] = self->adjacentChunks[WEST].lock();  g[0][1] = hold[0][1].get();
        hold[1][2] = self->adjacentChunks[NORTH].lock(); g[1][2] = hold[1][2].get();
        hold[1][0] = self->adjacentChunks[SOUTH].lock(); g[1][0] = hold[1][0].get();
        if (g[2][1]) {
            hold[2][2] = g[2][1]->adjacentChunks[NORTH].lock(); g[2][2] = hold[2][2].get();
            hold[2][0] = g[2][1]->adjacentChunks[SOUTH].lock(); g[2][0] = hold[2][0].get();
        }
        if (g[0][1]) {
            hold[0][2] = g[0][1]->adjacentChunks[NORTH].lock(); g[0][2] = hold[0][2].get();
            hold[0][0] = g[0][1]->adjacentChunks[SOUTH].lock(); g[0][0] = hold[0][0].get();
        }
    }

    Chunk* resolve(int rx, int ry, int rz, int& ox, int& oy, int& oz) {
        if (ry < 0 || ry >= HEIGHT) return nullptr;
        const int cox = rx < 0 ? -1 : (rx >= WIDTH ? 1 : 0);
        const int coz = rz < 0 ? -1 : (rz >= DEPTH ? 1 : 0);
        Chunk* c = g[cox + 1][coz + 1];
        if (!c) return nullptr;
        ox = rx - cox * WIDTH; oy = ry; oz = rz - coz * DEPTH;
        if (ox < 0 || ox >= WIDTH || oz < 0 || oz >= DEPTH) return nullptr;
        return c;
    }
    BlockType blk(int rx, int ry, int rz) {
        int x, y, z; Chunk* c = resolve(rx, ry, rz, x, y, z);
        return c ? c->getBlock(x, y, z) : BlockType::AIR;
    }
    int light(int rx, int ry, int rz) {
        int x, y, z; Chunk* c = resolve(rx, ry, rz, x, y, z);
        if (!c || c->blockLight.empty()) return 0;
        return c->blockLight[x + WIDTH * (y + HEIGHT * z)];
    }
    void setLight(int rx, int ry, int rz, int v) {
        int x, y, z; Chunk* c = resolve(rx, ry, rz, x, y, z);
        if (!c) return;
        if (c->blockLight.empty()) c->blockLight.assign(BLOCK_COUNT, 0);
        c->blockLight[x + WIDTH * (y + HEIGHT * z)] = (uint8_t)v;
        const int cox = rx < 0 ? -1 : (rx >= WIDTH ? 1 : 0);
        const int coz = rz < 0 ? -1 : (rz >= DEPTH ? 1 : 0);
        touched[cox + 1][coz + 1] = true;
    }
};

void Chunk::addBlockLightIncremental(int lx, int ly, int lz, bool touched[3][3])
{
    IncLightCtx ctx(this, touched);
    struct N { int x, y, z; };
    std::queue<N> q;

    ctx.setLight(lx, ly, lz, 14);
    q.push({ lx, ly, lz });
    while (!q.empty()) {
        N p = q.front(); q.pop();
        const int lv = ctx.light(p.x, p.y, p.z);
        if (lv <= 1) continue;
        const int s = lv - 1;
        for (int d = 0; d < 6; ++d) {
            const int nx = p.x + LDX[d], ny = p.y + LDY[d], nz = p.z + LDZ[d];
            if (!lightPasses(ctx.blk(nx, ny, nz))) continue;
            if (ctx.light(nx, ny, nz) < s) {
                ctx.setLight(nx, ny, nz, s);
                q.push({ nx, ny, nz });
            }
        }
    }
    touched[1][1] = true; // host chunk's mesh rebuilds for the block change
}

void Chunk::removeBlockLightIncremental(int lx, int ly, int lz, bool touched[3][3])
{
    IncLightCtx ctx(this, touched);
    struct D { int x, y, z, lv; };
    struct N { int x, y, z; };
    std::queue<D> dark;
    std::queue<N> relight;

    const int lv0 = ctx.light(lx, ly, lz);
    ctx.setLight(lx, ly, lz, 0);
    dark.push({ lx, ly, lz, lv0 });

    // 1
    while (!dark.empty()) {
        D p = dark.front(); dark.pop();
        for (int d = 0; d < 6; ++d) {
            const int nx = p.x + LDX[d], ny = p.y + LDY[d], nz = p.z + LDZ[d];
            const int nlv = ctx.light(nx, ny, nz);
            if (nlv == 0) continue;
            if (nlv < p.lv) {
                ctx.setLight(nx, ny, nz, 0);
                dark.push({ nx, ny, nz, nlv });
            } else {
                relight.push({ nx, ny, nz }); // still lit elsewhere
            }
        }
    }

    // 2
    while (!relight.empty()) {
        N p = relight.front(); relight.pop();
        const int lv = ctx.light(p.x, p.y, p.z);
        if (lv <= 1) continue;
        const int s = lv - 1;
        for (int d = 0; d < 6; ++d) {
            const int nx = p.x + LDX[d], ny = p.y + LDY[d], nz = p.z + LDZ[d];
            if (!lightPasses(ctx.blk(nx, ny, nz))) continue;
            if (ctx.light(nx, ny, nz) < s) {
                ctx.setLight(nx, ny, nz, s);
                relight.push({ nx, ny, nz });
            }
        }
    }

    // The removed torch may not have been this chunk's only one
    bool still = false;
    for (int x = 0; x < WIDTH && !still; ++x)
        for (int z = 0; z < DEPTH && !still; ++z)
            for (int y = 0; y < HEIGHT; ++y)
                if (isTorch(getBlock(x, y, z))) { still = true; break; }
    containsTorch = still;
    touched[1][1] = true;
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

    // Biome map — serialised as fixed-width uint8_t, independent of BiomeType's in-memory size
    for (const auto& b : biomeMap) {
        uint8_t v = static_cast<uint8_t>(b);
        out.write(reinterpret_cast<const char*>(&v), sizeof(v));
    }

	// --- Save vegetation ---
	uint32_t vegetationCount = static_cast<uint32_t>(vegetation.size());
	out.write(reinterpret_cast<const char*>(&vegetationCount), sizeof(vegetationCount));

	for (const auto& veg : vegetation) {
		out.write(reinterpret_cast<const char*>(&veg.x), sizeof(veg.x));
		out.write(reinterpret_cast<const char*>(&veg.y), sizeof(veg.y));
		out.write(reinterpret_cast<const char*>(&veg.z), sizeof(veg.z));
		out.write(reinterpret_cast<const char*>(&veg.type), sizeof(veg.type));
        out.write(reinterpret_cast<const char*>(&veg.biome), sizeof(veg.biome));
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

    // --- Load biome map ---
    for (auto& b : biomeMap) {
        uint8_t v = 0;
        in.read(reinterpret_cast<char*>(&v), sizeof(v));
        b = static_cast<BiomeType>(v);
    }

	// --- Load vegetation ---
	uint32_t vegetationCount = 0;
	in.read(reinterpret_cast<char*>(&vegetationCount), sizeof(vegetationCount));

    if (!in.good()) {
        // Older or truncated file: no vegetation block present.
        in.clear(); // clear eof/fail so caller can continue using stream
        vegetationCount = 0;
    } else {
        // Safety cap: one chunk can't reasonably hold more than ~8 vegetation per column.
        constexpr uint32_t kMaxVegetationPerChunk = 2048;
        if (vegetationCount > kMaxVegetationPerChunk) {
            vegetationCount = kMaxVegetationPerChunk;
        }
    }

	vegetation.reserve(vegetationCount);

	for (uint32_t i = 0; i < vegetationCount; ++i) {
		VegetationInstance veg{};

		in.read(reinterpret_cast<char*>(&veg.x), sizeof(veg.x));
		in.read(reinterpret_cast<char*>(&veg.y), sizeof(veg.y));
		in.read(reinterpret_cast<char*>(&veg.z), sizeof(veg.z));
		in.read(reinterpret_cast<char*>(&veg.type), sizeof(veg.type));
        in.read(reinterpret_cast<char*>(&veg.biome), sizeof(veg.biome));

        if (!in.good()) {
            // Corrupt/truncated entry list: keep what we already read.
            in.clear();
            break;
        }

		vegetation.push_back(veg);
        
		// Don't overwrite water blocks with sea vegetation —
		// sea vegetation is rendered purely via the vegetation renderer
		if (!isSeaVegetation(veg.type)) {
			setBlock(veg.x, veg.y, veg.z, veg.type);
		}
	}
}

BiomeType Chunk::getBiomeAt(int localX, int localZ) const {
    if (localX < 0 || localX >= WIDTH || localZ < 0 || localZ >= DEPTH)
        return BiomeType::PLAINS;
    return biomeMap[localX + WIDTH * localZ];
}


void Chunk::setBiomeAt(int localX, int localZ, BiomeType biome) {
    if (localX < 0 || localX >= WIDTH || localZ < 0 || localZ >= DEPTH)
        return;
    biomeMap[localX + WIDTH * localZ] = biome;
}
