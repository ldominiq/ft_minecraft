#include "ChunkBoundaryRenderer.hpp"
#include "Renderer.hpp"
#include <vector>
#include <cmath>

// ---------------------------------------------------------------------------
// The chunk boundary is drawn as a full grid on the four walls of the current
// chunk, from Y=0 to Y=HEIGHT.  Depth testing ensures blocks occlude the
// lines behind them.
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

    const float ox = static_cast<float>(chunkPtr->getOriginX());
    const float oz = static_cast<float>(chunkPtr->getOriginZ());
    const float W  = static_cast<float>(Chunk::WIDTH);
    const float D  = static_cast<float>(Chunk::DEPTH);
    const float H  = static_cast<float>(Chunk::HEIGHT);

    std::vector<glm::vec3> verts;

    verts.reserve(
        4 * (Chunk::WIDTH + 1) * 2 +       // vertical lines
        4 * (Chunk::HEIGHT + 1) * 2         // horizontal lines
    );

    // ── Vertical lines (Y=0 to Y=HEIGHT) ───────────────────────────

    // North wall (z = oz)
    for (int i = 0; i <= Chunk::WIDTH; ++i) {
        float x = ox + static_cast<float>(i);
        verts.push_back({x, 0.0f, oz});
        verts.push_back({x, H,    oz});
    }
    // South wall (z = oz + D)
    for (int i = 0; i <= Chunk::WIDTH; ++i) {
        float x = ox + static_cast<float>(i);
        verts.push_back({x, 0.0f, oz + D});
        verts.push_back({x, H,    oz + D});
    }
    // West wall (x = ox)
    for (int j = 0; j <= Chunk::DEPTH; ++j) {
        float z = oz + static_cast<float>(j);
        verts.push_back({ox, 0.0f, z});
        verts.push_back({ox, H,    z});
    }
    // East wall (x = ox + W)
    for (int j = 0; j <= Chunk::DEPTH; ++j) {
        float z = oz + static_cast<float>(j);
        verts.push_back({ox + W, 0.0f, z});
        verts.push_back({ox + W, H,    z});
    }

    // ── Horizontal lines (one per Y level) ──────────────────────────
    for (int y = 0; y <= Chunk::HEIGHT; ++y) {
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

    int chunkX = static_cast<int>(std::floor(playerPos.x / Chunk::WIDTH));
    int chunkZ = static_cast<int>(std::floor(playerPos.z / Chunk::DEPTH));

    if (chunkX != lastChunkX || chunkZ != lastChunkZ) {
        buildGrid(chunkX, chunkZ, renderer);
        lastChunkX = chunkX;
        lastChunkZ = chunkZ;
    }

    if (vertexCount == 0) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
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
