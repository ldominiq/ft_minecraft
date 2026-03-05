#include "ChunkBoundaryRenderer.hpp"
#include "Renderer.hpp"
#include <vector>
#include <cmath>
#include <algorithm>

// ---------------------------------------------------------------------------
// The chunk boundary is drawn as a grid on the four walls of the current
// chunk.  For each column along a wall edge, we scan downward through the
// chunk's block data to find the highest solid block (the surface).  Vertical
// lines start at that surface Y and go up to Chunk::HEIGHT.  Horizontal lines
// are drawn at every integer Y from the minimum surface height on each wall
// up to Chunk::HEIGHT, forming a proper grid.
// ---------------------------------------------------------------------------


ChunkBoundaryRenderer::ChunkBoundaryRenderer() {
    initGL();
}

ChunkBoundaryRenderer::~ChunkBoundaryRenderer() {
    if (VAO) glDeleteVertexArrays(1, &VAO);
    if (VBO) glDeleteBuffers(1, &VBO);
}

void ChunkBoundaryRenderer::initGL() {
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    shader = std::make_unique<Shader>("shaders/chunkBoundary.vert", "shaders/chunkBoundary.frag");
}

void ChunkBoundaryRenderer::buildGrid(int chunkX, int chunkZ, Renderer& renderer) {
    auto chunkPtr = renderer.getChunk(chunkX, chunkZ);
    if (!chunkPtr) {
        vertexCount = 0;
        return;
    }
    Chunk& chunk = *chunkPtr;

    const float ox = static_cast<float>(chunk.getOriginX());
    const float oz = static_cast<float>(chunk.getOriginZ());
    const float W  = static_cast<float>(Chunk::WIDTH);
    const float D  = static_cast<float>(Chunk::DEPTH);
    const float H  = static_cast<float>(Chunk::HEIGHT);

    // Surface heights for the 4 edges (north z=0, south z=15, west x=0, east x=15)
    int surfNorth[Chunk::WIDTH]; // indexed by local x
    int surfSouth[Chunk::WIDTH];
    int surfWest[Chunk::DEPTH];  // indexed by local z
    int surfEast[Chunk::DEPTH];

    // Find the minimum surface Y across all edges to determine where
    // horizontal lines should start.
    int minSurf = Chunk::HEIGHT;
    for (int i = 0; i < Chunk::WIDTH; ++i) {
        if (surfNorth[i] >= 0) minSurf = std::min(minSurf, surfNorth[i]);
        if (surfSouth[i] >= 0) minSurf = std::min(minSurf, surfSouth[i]);
    }
    for (int j = 0; j < Chunk::DEPTH; ++j) {
        if (surfWest[j] >= 0) minSurf = std::min(minSurf, surfWest[j]);
        if (surfEast[j] >= 0) minSurf = std::min(minSurf, surfEast[j]);
    }
    if (minSurf >= Chunk::HEIGHT) {
        vertexCount = 0;
        return;
    }

    std::vector<glm::vec3> verts;

    // Reserve a generous estimate: 4 walls × 17 vertical lines + many horizontal lines
    verts.reserve(
        4 * (Chunk::WIDTH + 1) * 2 +   // vertical lines
        4 * (Chunk::HEIGHT - minSurf) * 2 // horizontal lines
    );

    // ── Vertical lines ──────────────────────────────────────────────
    // Each vertical line starts at the surface of that column.

    // North wall (z = oz, local z = 0): one vertical per block boundary
    for (int i = 0; i <= Chunk::WIDTH; ++i) {
        int surfL = (i > 0)              ? surfNorth[i - 1] : surfNorth[0];
        int surfR = (i < Chunk::WIDTH)   ? surfNorth[i]     : surfNorth[Chunk::WIDTH - 1];
        int surf  = std::min(surfL, surfR);
        if (surf < 0) surf = minSurf;
        float x = ox + static_cast<float>(i);
        verts.push_back({x, static_cast<float>(surf), oz});
        verts.push_back({x, H, oz});
    }

    // South wall (z = oz + D, local z = DEPTH-1)
    for (int i = 0; i <= Chunk::WIDTH; ++i) {
        int surfL = (i > 0)              ? surfSouth[i - 1] : surfSouth[0];
        int surfR = (i < Chunk::WIDTH)   ? surfSouth[i]     : surfSouth[Chunk::WIDTH - 1];
        int surf  = std::min(surfL, surfR);
        if (surf < 0) surf = minSurf;
        float x = ox + static_cast<float>(i);
        verts.push_back({x, static_cast<float>(surf), oz + D});
        verts.push_back({x, H, oz + D});
    }

    // West wall (x = ox, local x = 0)
    for (int j = 0; j <= Chunk::DEPTH; ++j) {
        int surfL = (j > 0)             ? surfWest[j - 1] : surfWest[0];
        int surfR = (j < Chunk::DEPTH)  ? surfWest[j]     : surfWest[Chunk::DEPTH - 1];
        int surf  = std::min(surfL, surfR);
        if (surf < 0) surf = minSurf;
        float z = oz + static_cast<float>(j);
        verts.push_back({ox, static_cast<float>(surf), z});
        verts.push_back({ox, H, z});
    }

    // East wall (x = ox + W, local x = WIDTH-1)
    for (int j = 0; j <= Chunk::DEPTH; ++j) {
        int surfL = (j > 0)             ? surfEast[j - 1] : surfEast[0];
        int surfR = (j < Chunk::DEPTH)  ? surfEast[j]     : surfEast[Chunk::DEPTH - 1];
        int surf  = std::min(surfL, surfR);
        if (surf < 0) surf = minSurf;
        float z = oz + static_cast<float>(j);
        verts.push_back({ox + W, static_cast<float>(surf), z});
        verts.push_back({ox + W, H, z});
    }

    // ── Horizontal lines (complete the grid) ────────────────────────
    // One horizontal line per Y level, per wall, from minSurf to HEIGHT.
    for (int y = minSurf; y <= Chunk::HEIGHT; ++y) {
        float fy = static_cast<float>(y);
        // North
        verts.push_back({ox,     fy, oz});
        verts.push_back({ox + W, fy, oz});
        // South
        verts.push_back({ox,     fy, oz + D});
        verts.push_back({ox + W, fy, oz + D});
        // West
        verts.push_back({ox, fy, oz});
        verts.push_back({ox, fy, oz + D});
        // East
        verts.push_back({ox + W, fy, oz});
        verts.push_back({ox + W, fy, oz + D});
    }

    vertexCount = static_cast<unsigned int>(verts.size());

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(verts.size() * sizeof(glm::vec3)),
                 verts.data(), GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

void ChunkBoundaryRenderer::draw(const glm::vec3& playerPos,
                                  const glm::mat4& view,
                                  const glm::mat4& projection,
                                  Renderer& renderer) {
    if (!enabled) return;

    // Determine which chunk the player is in
    int chunkX = static_cast<int>(std::floor(playerPos.x / Chunk::WIDTH));
    int chunkZ = static_cast<int>(std::floor(playerPos.z / Chunk::DEPTH));

    // Rebuild geometry only when the player enters a different chunk
    if (chunkX != lastChunkX || chunkZ != lastChunkZ) {
        buildGrid(chunkX, chunkZ, renderer);
        lastChunkX = chunkX;
        lastChunkZ = chunkZ;
    }

    if (vertexCount == 0) return;

    // Draw with blending for translucency, depth-tested so blocks occlude the grid
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE); // don't write to depth buffer (translucent overlay)
    glLineWidth(1.0f);

    shader->use();
    shader->setMat4("view", view);
    shader->setMat4("projection", projection);
    shader->setVec4("lineColor", glm::vec4(1.0f, 1.0f, 0.0f, 0.25f)); // soft yellow

    glBindVertexArray(VAO);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertexCount));
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
