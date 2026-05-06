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

    // Verts are stored chunk-local (origin at chunk corner). The chunk's world
    // origin is added per-frame in the shader via a uniform offset, so this
    // mesh stays valid no matter how far from world origin the player is.
    const float W  = static_cast<float>(Chunk::WIDTH);
    const float D  = static_cast<float>(Chunk::DEPTH);
    const float H  = static_cast<float>(Chunk::HEIGHT);

    std::vector<glm::vec3> verts;

    verts.reserve(
        4 * (Chunk::WIDTH + 1) * 2 +       // vertical lines
        4 * (Chunk::HEIGHT + 1) * 2         // horizontal lines
    );

    // ── Vertical lines  ───────────────────────────

    // North wall (z = 0)
    for (int i = 0; i <= Chunk::WIDTH; ++i) {
        float x = static_cast<float>(i);
        verts.push_back({x, 0.0f, 0.0f});
        verts.push_back({x, H,    0.0f});
    }
    // South wall (z = D)
    for (int i = 0; i <= Chunk::WIDTH; ++i) {
        float x = static_cast<float>(i);
        verts.push_back({x, 0.0f, D});
        verts.push_back({x, H,    D});
    }
    // West wall (x = 0)
    for (int j = 0; j <= Chunk::DEPTH; ++j) {
        float z = static_cast<float>(j);
        verts.push_back({0.0f, 0.0f, z});
        verts.push_back({0.0f, H,    z});
    }
    // East wall (x = W)
    for (int j = 0; j <= Chunk::DEPTH; ++j) {
        float z = static_cast<float>(j);
        verts.push_back({W, 0.0f, z});
        verts.push_back({W, H,    z});
    }

    // ── Horizontal lines
    for (int y = 0; y <= Chunk::HEIGHT; ++y) {
        float fy = static_cast<float>(y);
        // North
        verts.push_back({0.0f, fy, 0.0f});
        verts.push_back({W,    fy, 0.0f});
        // South
        verts.push_back({0.0f, fy, D});
        verts.push_back({W,    fy, D});
        // West
        verts.push_back({0.0f, fy, 0.0f});
        verts.push_back({0.0f, fy, D});
        // East
        verts.push_back({W, fy, 0.0f});
        verts.push_back({W, fy, D});
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
                                  const glm::dvec3& eyePos,
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

    // Camera-relative chunk origin, computed in double so the cancellation is
    // precise even at very large world coordinates.
    const glm::dvec3 chunkOriginD(static_cast<double>(chunkX) * Chunk::WIDTH, 0.0,
                                  static_cast<double>(chunkZ) * Chunk::DEPTH);
    const glm::vec3 chunkRel = glm::vec3(chunkOriginD - eyePos);

    glm::mat4 viewRot = view;
    viewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

    shader->use();
    shader->setMat4("view", viewRot);
    shader->setMat4("projection", projection);
    shader->setVec3("chunkRel", chunkRel);
    shader->setVec4("lineColor", glm::vec4(1.0f, 1.0f, 0.0f, 0.25f)); // soft yellow

    glBindVertexArray(VAO);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(vertexCount));
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}
