#include "VegetationRenderer.hpp"
#include "Item.hpp"
#include <cmath>
#include <unordered_map>

VegetationRenderer::VegetationRenderer() {}

VegetationRenderer::~VegetationRenderer() {
    releaseGL();
}

void VegetationRenderer::releaseGL() {
    if (glfwGetCurrentContext()) {
        if (VAO) glDeleteVertexArrays(1, &VAO);
        if (VBO) glDeleteBuffers(1, &VBO);
        if (instanceVBO) glDeleteBuffers(1, &instanceVBO);
    }
    VAO = VBO = instanceVBO = 0;
}

std::vector<float> VegetationRenderer::generateCrossPatternMesh() {
    // Generate a cross-pattern mesh (2 quads forming an X when viewed from above)
    // Each vertex: position (3) + texCoord (2) + normal (3) = 8 floats
    // Each quad: 6 vertices (2 triangles)
    // Total: 2 quads * 6 vertices * 8 floats = 96 floats

    constexpr float w = 0.4f;       // Half-width (0.8/2)
    constexpr float h = 1.0f;       // Height of vegetation
    constexpr float n1 = 0.707f;    // Normal component (1/sqrt(2))

    // pos(3), tex(2), normal(3) × 12 vertices
    return {
        // Quad 1: diagonal NW-SE
        -w, 0.0f, -w,  0.0f, 0.0f,   n1, 0.0f,  n1,  // v0
         w, 0.0f,  w,  1.0f, 0.0f,   n1, 0.0f,  n1,  // v1
         w, h,     w,  1.0f, 1.0f,   n1, 0.0f,  n1,  // v2
         w, h,     w,  1.0f, 1.0f,   n1, 0.0f,  n1,  // v2
        -w, h,    -w,  0.0f, 1.0f,   n1, 0.0f,  n1,  // v3
        -w, 0.0f, -w,  0.0f, 0.0f,   n1, 0.0f,  n1,  // v0

        // Quad 2: diagonal NE-SW
         w, 0.0f, -w,  0.0f, 0.0f,  -n1, 0.0f,  n1,  // v4
        -w, 0.0f,  w,  1.0f, 0.0f,  -n1, 0.0f,  n1,  // v5
        -w, h,     w,  1.0f, 1.0f,  -n1, 0.0f,  n1,  // v6
        -w, h,     w,  1.0f, 1.0f,  -n1, 0.0f,  n1,  // v6
         w, h,    -w,  0.0f, 1.0f,  -n1, 0.0f,  n1,  // v7
         w, 0.0f, -w,  0.0f, 0.0f,  -n1, 0.0f,  n1,  // v4
    };
}

void VegetationRenderer::buildInstances(const Chunk::VegetationInstance* instances, size_t count,
                                        int chunkOriginX, int chunkOriginZ, const Chunk* chunk) {
    // Build instance data: chunk-local position (3), texture layer (1), rotation (1), skylight (1), columnBaseY (1), aoFactor (1), blockLight (1) = 9 floats per instance

    // First pass: for sea vegetation, find the lowest Y per column (x,z) to use as column base
    // so the shader can compute coherent sway for stacked blocks.
    std::unordered_map<uint32_t, uint8_t> columnBaseMap; // key = (x << 16 | z), value = min Y
    for (size_t i = 0; i < count; ++i) {
        const auto& veg = instances[i];
        if (isSeaVegetation(veg.type)) {
            uint32_t key = (static_cast<uint32_t>(veg.x) << 16) | static_cast<uint32_t>(veg.z);
            auto it = columnBaseMap.find(key);
            if (it == columnBaseMap.end() || veg.y < it->second) {
                columnBaseMap[key] = veg.y;
            }
        }
    }

    std::vector<float> instanceData;
    instanceData.reserve(count * 9);

    for (size_t i = 0; i < count; ++i) {
        const auto& veg = instances[i];

        constexpr float offset = 0.5f;
        // Keep instance X/Z chunk-local for precision; world origin is supplied separately per chunk.
        const int worldBlockX = chunkOriginX + static_cast<int>(veg.x);
        const int worldBlockZ = chunkOriginZ + static_cast<int>(veg.z);
        float worldY = static_cast<float>(veg.y);
        float localX = static_cast<float>(veg.x) + offset;
        float localZ = static_cast<float>(veg.z) + offset;

        // Get texture layer for this vegetation type
        const auto& textures = textureManager->getBlockTextures(veg.type);
        float texLayer = static_cast<float>(textures.top);
        if (veg.type == BlockType::SHORT_GRASS) {
            texLayer = textureManager->getShortGrassTintLayer(static_cast<BiomeType>(veg.biome));
        }

        // Random rotation for variety (seeded by position for determinism).
        // Uses a cheap integer hash instead of mt19937 to avoid per-instance state init overhead.
        // The whole thing is ~5 CPU instructions vs. 624 array writes for mt19937..
        // Mix X and Z together using two different large primes
        uint32_t h = static_cast<uint32_t>(worldBlockX * 73856093)
                   ^ static_cast<uint32_t>(worldBlockZ * 19349663);

        // Three-step finalizer: each step breaks up linear patterns left by the multiply
        h ^= h >> 16; // fold high bits onto low bits
        h *= 0x45d9f3bu; // scramble with another prime
        h ^= h >> 16; // fold again

        // Take the bottom 16 bits, scale to [0, 2π]
        // h & 0xFFFF extracts the bottom 16 bits (values 0–65535), dividing by 65536 gives a float in [0, 1), multiplying by 2π gives an angle in [0, 2π).
        float rotation = static_cast<float>(h & 0xFFFFu) / 65536.0f * 2.0f * 3.14159265f;

        // Get sky-light from chunk data
        float skyLightVal = 1.0f;
        if (chunk) {
            skyLightVal = static_cast<float>(chunk->getSkyLight(veg.x, veg.y, veg.z)) / 15.0f;
        }

        // Column base Y: for sea vegetation, use the lowest block in the column;
        // for land vegetation, use the instance's own Y (single block, irrelevant)
        float columnBaseY = worldY;
        if (isSeaVegetation(veg.type)) {
            uint32_t key = (static_cast<uint32_t>(veg.x) << 16) | static_cast<uint32_t>(veg.z);
            // map[key] does two things if the key doesn't exist: it inserts a default value (0) and then returns it
            // we want to avoid inserting a default value for missing keys, so now we use find() instead
            auto baseIt = columnBaseMap.find(key);
            columnBaseY = static_cast<float>(baseIt != columnBaseMap.end() ? baseIt->second : veg.y);
        }

        // Compute ambient occlusion factor with vertical sky visibility check
        float aoFactor = 1.0f;
        if (chunk) {
            // 1. Check immediate neighbors (6 directions) for enclosed spaces
            int solidCount = 0;
            const int offsets[6][3] = {{1,0,0}, {-1,0,0}, {0,1,0}, {0,-1,0}, {0,0,1}, {0,0,-1}};
            for (int d = 0; d < 6; ++d) {
                int nx = veg.x + offsets[d][0];
                int ny = veg.y + offsets[d][1];
                int nz = veg.z + offsets[d][2];
                BlockType neighbor = chunk->getBlock(nx, ny, nz);
                if (isBlockSolid(neighbor)) {
                    solidCount++;
                }
            }
            float enclosureAO = 1.0f - (static_cast<float>(solidCount) / 6.0f) * 0.4f;

            // 2. Check vertical sky visibility (reduced impact since CSM handles shadows)
            // Look upward in steps, counting opaque/semi-opaque blocks
            int blockedCount = 0;
            constexpr int checkHeight = 6; // Reduced from 8 (CSM is more accurate)
            for (int dy = 1; dy <= checkHeight; ++dy) {
                BlockType above = chunk->getBlock(veg.x, veg.y + dy, veg.z);
                // Count solid blocks and leaves (leaves partially block light)
                if (isBlockSolid(above)) {
                    blockedCount += 2; // Solid blocks block more
                } else if (isBlockTransparent(above) && above != BlockType::AIR) {
                    blockedCount += 1; // Leaves partially block
                }
            }
            // Reduced impact: 0 = 1.0 (bright), 12 = 0.7 (light shadow, CSM does the rest)
            float skyVisibility = 1.0f - std::min(static_cast<float>(blockedCount) / 12.0f, 0.3f);

            // Combine both factors: enclosure darkening + light overhead shadowing
            // CSM will handle accurate sun-based shadows, AO just prevents vegetation being too bright
            aoFactor = enclosureAO * skyVisibility;
        }

        // Note: Block light is not implemented in the current chunk system
        // For now, default to 0 (no block light) - can be extended when torches are added
        float blockLightVal = 0.0f;

        instanceData.push_back(localX);
        instanceData.push_back(worldY);
        instanceData.push_back(localZ);
        instanceData.push_back(texLayer);
        instanceData.push_back(rotation);
        instanceData.push_back(skyLightVal);
        instanceData.push_back(columnBaseY);
        instanceData.push_back(aoFactor);
        instanceData.push_back(blockLightVal);
    }

    instanceCount = static_cast<uint32_t>(count);

    // Upload instance data to GPU
    if (instanceVBO == 0)
        glGenBuffers(1, &instanceVBO);

    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);
    glBufferData(GL_ARRAY_BUFFER, instanceData.size() * sizeof(float), instanceData.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void VegetationRenderer::uploadMesh() {
    if (instanceCount == 0)
        return;

    // Generate VAO and VBO if needed
    if (VAO == 0)
        glGenVertexArrays(1, &VAO);
    if (VBO == 0)
        glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);

    // Upload base mesh (cross-pattern)
    auto meshData = generateCrossPatternMesh();
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, meshData.size() * sizeof(float), meshData.data(), GL_STATIC_DRAW);

    // Vertex attributes (per-vertex data)
    GLsizei stride = 8 * sizeof(float); // pos(3) + texCoord(2) + normal(3)

    // Location 0: position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, static_cast<void *>(nullptr));
    glEnableVertexAttribArray(0);

    // Location 1: texCoord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Location 2: normal
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void *>(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    // Instance attributes (per-instance data)
    glBindBuffer(GL_ARRAY_BUFFER, instanceVBO);

    constexpr GLsizei instanceStride = 9 * sizeof(float); // localPos(3) + texLayer(1) + rotation(1) + skylight(1) + columnBaseY(1) + aoFactor(1) + blockLight(1)

    // Location 3: instance position (vec3)
    glVertexAttribPointer(3, 3, GL_FLOAT, GL_FALSE, instanceStride, static_cast<void *>(nullptr));
    glEnableVertexAttribArray(3);
    glVertexAttribDivisor(3, 1); // Advance once per instance

    // Location 4: texture layer (float)
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, instanceStride, reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(4);
    glVertexAttribDivisor(4, 1);

    // Location 5: rotation (float)
    glVertexAttribPointer(5, 1, GL_FLOAT, GL_FALSE, instanceStride, reinterpret_cast<void *>(4 * sizeof(float)));
    glEnableVertexAttribArray(5);
    glVertexAttribDivisor(5, 1);

    // Location 6: skylight (float)
    glVertexAttribPointer(6, 1, GL_FLOAT, GL_FALSE, instanceStride, reinterpret_cast<void *>(5 * sizeof(float)));
    glEnableVertexAttribArray(6);
    glVertexAttribDivisor(6, 1);

    // Location 7: column base Y (float)
    glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, instanceStride, reinterpret_cast<void *>(6 * sizeof(float)));
    glEnableVertexAttribArray(7);
    glVertexAttribDivisor(7, 1);

    // Location 8: AO factor (float)
    glVertexAttribPointer(8, 1, GL_FLOAT, GL_FALSE, instanceStride, reinterpret_cast<void *>(7 * sizeof(float)));
    glEnableVertexAttribArray(8);
    glVertexAttribDivisor(8, 1);

    // Location 9: block light (float)
    glVertexAttribPointer(9, 1, GL_FLOAT, GL_FALSE, instanceStride, reinterpret_cast<void *>(8 * sizeof(float)));
    glEnableVertexAttribArray(9);
    glVertexAttribDivisor(9, 1);

    glBindVertexArray(0);
}

void VegetationRenderer::render() const {
    if (VAO == 0 || instanceCount == 0)
        return;

    // Disable face culling so both sides of quads are visible
    glDisable(GL_CULL_FACE);

    // No blending — rely on discard in fragment shader for transparent pixels.
    // Blending + depth writes causes transparent parts of the quad to occlude
    // terrain behind (see-through holes depending on draw order / angle).

    glBindVertexArray(VAO);
    glDrawArraysInstanced(GL_TRIANGLES, 0, 12, instanceCount); // 12 vertices (2 quads * 6 vertices)
    glBindVertexArray(0);

    // Restore default state
    glEnable(GL_CULL_FACE);
}

void VegetationRenderer::clearInstances() {
    instanceCount = 0;
}
