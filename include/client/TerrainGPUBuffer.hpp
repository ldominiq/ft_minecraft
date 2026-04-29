#ifndef TERRAIN_GPU_BUFFER_HPP
#define TERRAIN_GPU_BUFFER_HPP

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>
#include <list>
#include <span>
#include <array>
#include <memory>

#include "Shader.hpp"
#include "PackedVertex.hpp"

// ─────────────────────────────────────────────────────────────────────────────
// GPU-side data structs (must match GLSL std430 layout exactly).
// ─────────────────────────────────────────────────────────────────────────────

// Per-chunk metadata stored in SSBO binding 0.
// std430 layout: 3 × vec4 (48 bytes) + 4 × uint (16 bytes) = 64 bytes total.
//
// The vertex shaders read originWorld via raw vec4 indexing (terrain_ssbo.glsl).
// The compute shader reads everything via the typed struct (frustum_cull.comp).
struct alignas(16) GpuChunkInfo {
    glm::vec4 originWorld;  // .xyz = world-space chunk origin (same as cachedMinP)
    glm::vec4 aabbMin;      // .xyz = world-space AABB minimum corner
    glm::vec4 aabbMax;      // .xyz = world-space AABB maximum corner
    uint32_t  vertexFirst;  // starting index in the vertex SSBO (= gl_VertexID base)
    uint32_t  vertexCount;  // 0 means the slot is empty/unloaded; compute shader skips it
    uint32_t  _pad[2];
};
static_assert(sizeof(GpuChunkInfo) == 64, "GpuChunkInfo size mismatch — std430 layout broken");

// Draw command written by the compute shader, consumed by glMultiDrawArraysIndirect.
// Mirrors the OpenGL DrawArraysIndirectCommand layout exactly.
struct GpuDrawCommand {
    uint32_t count;          // vertex count (copy of ChunkInfo.vertexCount)
    uint32_t instanceCount;  // 1 = render this chunk, 0 = skip (culled or empty)
    uint32_t first;          // gl_VertexID base for this draw (= ChunkInfo.vertexFirst)
    uint32_t baseInstance;   // chunk slot index — the vertex shader reads it as gl_BaseInstance
                             // to index into the ChunkInfo SSBO for per-draw data
};
static_assert(sizeof(GpuDrawCommand) == 16, "GpuDrawCommand size mismatch");

// ─────────────────────────────────────────────────────────────────────────────
// TerrainGPUBuffer
// ─────────────────────────────────────────────────────────────────────────────
// Owns three GPU-side buffers that together replace the per-chunk VAO/VBO system:
//
//   SSBO binding 0 — ChunkInfo array (one slot per loaded chunk)
//   SSBO binding 1 — Packed vertex data for ALL chunks concatenated
//   SSBO binding 2 — Draw commands written by the compute shader each frame;
//                    also bound as GL_DRAW_INDIRECT_BUFFER for the MDI draw call
//
// Typical per-frame usage (Renderer side):
//   1.  terrainBuf.cull(cameraPos, frustumPlanes)   // compute shader fills SSBO 2
//   2.  shader->use(); shader->setXxx(...);          // bind terrain shader + set uniforms
//   3.  terrainBuf.bindSSBOs();                      // expose SSOBs 0 and 1 to the shader
//   4.  terrainBuf.draw();                           // single glMultiDrawArraysIndirect call
//
// Chunk lifecycle (ChunkRenderer side):
//   upload: slot = terrainBuf.alloc(verts, origin, aabbMin, aabbMax, outFirst)
//   remove: terrainBuf.free(slot, vertexFirst, vertexCount)
class TerrainGPUBuffer {
public:
    // Max simultaneous chunk slots. At render distance 64 chunks, ~12 868 chunks fit
    // inside the circle — 16 384 gives comfortable headroom.
    static constexpr uint32_t MAX_CHUNKS = 16384;

    // Max vertices across all loaded chunks (128 M × 8 B = 1 GB).
    // Radius-64 load circle holds ~12 800 chunks; at ~10k vertices each = 128 M.
    // glBufferData with nullptr commits virtual space only — VRAM is paged in lazily.
    static constexpr uint32_t MAX_VERTICES = 128u * 1024u * 1024u;

    // Returned by alloc() when the vertex pool or slot pool is full.
    static constexpr uint32_t INVALID_SLOT = UINT32_MAX;

    // ── Lifecycle ────────────────────────────────────────────────────────────

    // Allocate GPU buffers, create the empty VAO, compile the cull shader.
    // Must be called once on the OpenGL thread before any chunks are built.
    void init(std::shared_ptr<Shader> cullShader);

    // Release all GPU resources. Call before destroying the OpenGL context.
    void destroy();

    // ── Chunk slot management ─────────────────────────────────────────────────

    // Upload a chunk's vertices to the vertex SSBO and write its metadata to the
    // ChunkInfo SSBO. Returns the slot index on success, INVALID_SLOT if full.
    // outVertexFirst receives the starting vertex index within the SSBO.
    uint32_t alloc(std::span<const PackedVertex> vertices,
                   glm::vec3 originWorld, glm::vec3 aabbMin, glm::vec3 aabbMax,
                   uint32_t& outVertexFirst);

    // Return the slot and its vertex range to the free lists.
    // Zeros out ChunkInfo.vertexCount so the compute shader skips this slot next frame.
    void free(uint32_t slot, uint32_t vertexFirst, uint32_t vertexCount);

    // ── Per-frame GPU operations ──────────────────────────────────────────────

    // Dispatch the frustum-culling compute shader.
    //   cameraPos     — world position (float precision is fine for culling)
    //   frustumPlanes — 6 planes in camera-relative space (from Frustum::getPlanes())
    // After this returns (with a memory barrier), drawCmdSSBO is ready for MDI.
    void cull(const glm::vec3& cameraPos,
              const std::array<glm::vec4, 6>& frustumPlanes);

    // Bind ChunkInfo SSBO to binding 0 and vertex SSBO to binding 1.
    // Call this once before each terrain render pass so shaders can read chunk data.
    void bindSSBOs() const;

    // Bind drawCmdSSBO as GL_DRAW_INDIRECT_BUFFER and issue:
    //   glMultiDrawArraysIndirect(GL_TRIANGLES, nullptr, MAX_CHUNKS, 0)
    // Slots with instanceCount == 0 are skipped by the GPU at essentially zero cost.
    void draw() const;

    // Empty VAO — terrain draws use SSBO vertex pulling so no vertex attributes needed.
    GLuint emptyVAO = 0;

    // ── Global singleton ──────────────────────────────────────────────────────
    // App sets this pointer after init() and clears it before destroy().
    // ChunkRenderer and Renderer access it without needing a direct reference.
    static TerrainGPUBuffer* instance() { return s_ptr; }
    static void setInstance(TerrainGPUBuffer* p) { s_ptr = p; }

private:
    static TerrainGPUBuffer* s_ptr;

    GLuint m_chunkInfoSSBO = 0;  // binding 0: GpuChunkInfo[MAX_CHUNKS]
    GLuint m_vertexSSBO    = 0;  // binding 1: uvec2[MAX_VERTICES]  (= PackedVertex)
    GLuint m_drawCmdSSBO   = 0;  // binding 2: GpuDrawCommand[MAX_CHUNKS]

    std::shared_ptr<Shader> m_cullShader;

    // ── Chunk slot free list ──────────────────────────────────────────────────
    // Stack of available slot indices (0 .. MAX_CHUNKS-1).
    std::vector<uint32_t> m_freeSlots;

    // ── Vertex pool free list ─────────────────────────────────────────────────
    // Variable-size free blocks sorted by offset. Adjacent free blocks are merged
    // on deallocation to keep fragmentation low (first-fit allocation strategy).
    struct VBlock { uint32_t offset; uint32_t count; };
    std::list<VBlock> m_freeVertexBlocks;
};

#endif // TERRAIN_GPU_BUFFER_HPP
