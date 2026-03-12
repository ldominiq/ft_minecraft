#ifndef VEGETATION_RENDERER_HPP
#define VEGETATION_RENDERER_HPP

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <vector>
#include <cmath>
#include <random>
#include "Chunk.hpp"
#include "TextureManager.hpp"

class VegetationRenderer {
private:
    GLuint VAO = 0;
    GLuint VBO = 0;              // Vertex buffer (cross-pattern mesh)
    GLuint instanceVBO = 0;       // Instance buffer (positions, textures, rotations)
    uint32_t instanceCount = 0;

    const TextureManager* textureManager = nullptr;

    // Generate the base cross-pattern mesh (2 quads forming an X)
    static std::vector<float> generateCrossPatternMesh();

public:
    VegetationRenderer();
    ~VegetationRenderer();

    // Set the TextureManager (must be called before building)
    void setTextureManager(const TextureManager* tm) { textureManager = tm; }

    // Build instance buffer from chunk vegetation data
    void buildInstances(const Chunk::VegetationInstance* instances, size_t count, int chunkOriginX, int chunkOriginZ);

    // Upload mesh and instance data to GPU
    void uploadMesh();

    // Render all vegetation instances
    void render() const;

    // Release GL resources
    void releaseGL();

    inline GLuint getVAO() const { return VAO; }
    inline uint32_t getInstanceCount() const { return instanceCount; }
};

#endif
