#ifndef CHUNK_BOUNDARY_RENDERER_HPP
#define CHUNK_BOUNDARY_RENDERER_HPP

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>
#include "Shader.hpp"
#include "Chunk.hpp"

class Renderer;

/// Draws a translucent grid on the four walls of the chunk the player is
/// currently standing in.
class ChunkBoundaryRenderer {
public:
    ChunkBoundaryRenderer();
    ~ChunkBoundaryRenderer();

    /// Call once per frame.
    /// \p playerPos   current camera/player world position (used to find the chunk).
    /// \p view        current view matrix.
    /// \p projection  current projection matrix.
    /// \p renderer    the world renderer, used to read block data for surface detection.
    void draw(const glm::vec3& playerPos, const glm::mat4& view,
              const glm::mat4& projection, Renderer& renderer);

    bool isEnabled() const { return enabled; }
    void setEnabled(bool e) { enabled = e; }

private:
    void initGL();
    /// Scan the chunk's edge columns to find surface heights and build the grid.
    void buildGrid(int chunkX, int chunkZ, Renderer& renderer);

    GLuint VAO = 0;
    GLuint VBO = 0;
    unsigned int vertexCount = 0;

    std::unique_ptr<Shader> shader;
    bool enabled = false;

    // Cache to avoid rebuilding every frame
    int lastChunkX = INT_MIN;
    int lastChunkZ = INT_MIN;
};

#endif // CHUNK_BOUNDARY_RENDERER_HPP
