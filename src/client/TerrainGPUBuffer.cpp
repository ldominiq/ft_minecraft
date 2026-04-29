#include "TerrainGPUBuffer.hpp"
#include <cstring>
#include <iostream>
#include <algorithm>

TerrainGPUBuffer* TerrainGPUBuffer::s_ptr = nullptr;

// ─────────────────────────────────────────────────────────────────────────────
// init / destroy
// ─────────────────────────────────────────────────────────────────────────────

void TerrainGPUBuffer::init(std::shared_ptr<Shader> cullShader) {
    m_cullShader = std::move(cullShader);

    // ── Empty VAO ─────────────────────────────────────────────────────────────
    // Terrain uses SSBO vertex pulling — no vertex attribute arrays. We still need
    // a bound VAO because OpenGL requires one for any draw call.
    glGenVertexArrays(1, &emptyVAO);

    // ── ChunkInfo SSBO (binding 0) ────────────────────────────────────────────
    // Holds GpuChunkInfo for every chunk slot. Updated by alloc()/free().
    // The compute shader reads this each frame; vertex shaders also read it.
    glGenBuffers(1, &m_chunkInfoSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_chunkInfoSSBO);
    // Zero-initialize so empty slots have vertexCount = 0 and the cull shader skips them.
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 (GLsizeiptr)(MAX_CHUNKS * sizeof(GpuChunkInfo)),
                 nullptr, GL_DYNAMIC_DRAW);
    {
        // Zero the whole buffer on the GPU side.
        void* ptr = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_WRITE_ONLY);
        if (ptr) { memset(ptr, 0, MAX_CHUNKS * sizeof(GpuChunkInfo)); glUnmapBuffer(GL_SHADER_STORAGE_BUFFER); }
    }

    // ── Vertex SSBO (binding 1) ───────────────────────────────────────────────
    // All chunk vertices concatenated. Each element is a uvec2 (PackedVertex v0,v1).
    // Pre-allocate the full 512 MB — the GPU commits memory lazily on most drivers.
    glGenBuffers(1, &m_vertexSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_vertexSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 (GLsizeiptr)(MAX_VERTICES * sizeof(PackedVertex)),
                 nullptr, GL_DYNAMIC_DRAW);

    // ── Draw command SSBO (binding 2) ─────────────────────────────────────────
    // Written by the compute shader each frame, then bound as GL_DRAW_INDIRECT_BUFFER
    // so glMultiDrawArraysIndirect reads commands directly from GPU memory.
    glGenBuffers(1, &m_drawCmdSSBO);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_drawCmdSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER,
                 (GLsizeiptr)(MAX_CHUNKS * sizeof(GpuDrawCommand)),
                 nullptr, GL_DYNAMIC_DRAW);

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // ── Chunk slot free list ──────────────────────────────────────────────────
    // Fill the stack with all slot indices (highest first so pop gives the lowest).
    m_freeSlots.reserve(MAX_CHUNKS);
    for (int32_t i = (int32_t)MAX_CHUNKS - 1; i >= 0; --i)
        m_freeSlots.push_back((uint32_t)i);

    // ── Vertex pool free list ─────────────────────────────────────────────────
    // Start with one giant free block covering the whole vertex pool.
    m_freeVertexBlocks.push_back({ 0u, MAX_VERTICES });
}

void TerrainGPUBuffer::destroy() {
    if (emptyVAO)      { glDeleteVertexArrays(1, &emptyVAO);   emptyVAO = 0; }
    if (m_chunkInfoSSBO) { glDeleteBuffers(1, &m_chunkInfoSSBO); m_chunkInfoSSBO = 0; }
    if (m_vertexSSBO)    { glDeleteBuffers(1, &m_vertexSSBO);    m_vertexSSBO = 0; }
    if (m_drawCmdSSBO)   { glDeleteBuffers(1, &m_drawCmdSSBO);   m_drawCmdSSBO = 0; }
    m_freeSlots.clear();
    m_freeVertexBlocks.clear();
    m_cullShader.reset();
}

// ─────────────────────────────────────────────────────────────────────────────
// alloc
// ─────────────────────────────────────────────────────────────────────────────

uint32_t TerrainGPUBuffer::alloc(std::span<const PackedVertex> vertices,
                                  glm::vec3 originWorld,
                                  glm::vec3 aabbMin,
                                  glm::vec3 aabbMax,
                                  uint32_t& outVertexFirst) {
    const uint32_t numVerts = (uint32_t)vertices.size();

    // ── Grab a chunk slot ─────────────────────────────────────────────────────
    if (m_freeSlots.empty()) {
        std::cerr << "TerrainGPUBuffer: out of chunk slots (MAX_CHUNKS=" << MAX_CHUNKS << ")\n";
        return INVALID_SLOT;
    }
    uint32_t slot = m_freeSlots.back();
    m_freeSlots.pop_back();

    // ── Find a vertex block (first-fit) ──────────────────────────────────────
    // Even empty chunks (numVerts == 0) need a slot, but no vertex space.
    uint32_t vertexFirst = 0;
    if (numVerts > 0) {
        auto it = m_freeVertexBlocks.begin();
        for (; it != m_freeVertexBlocks.end(); ++it) {
            if (it->count >= numVerts) break;
        }
        if (it == m_freeVertexBlocks.end()) {
            std::cerr << "TerrainGPUBuffer: vertex SSBO full (MAX_VERTICES=" << MAX_VERTICES << ")\n";
            m_freeSlots.push_back(slot); // return slot
            return INVALID_SLOT;
        }
        vertexFirst = it->offset;

        // Upload vertex data into the SSBO at the allocated offset.
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_vertexSSBO);
        glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                        (GLintptr)(vertexFirst * sizeof(PackedVertex)),
                        (GLsizeiptr)(numVerts  * sizeof(PackedVertex)),
                        vertices.data());
        glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

        // Shrink the free block (or erase it if entirely consumed).
        if (it->count == numVerts)
            m_freeVertexBlocks.erase(it);
        else {
            it->offset += numVerts;
            it->count  -= numVerts;
        }
    }
    outVertexFirst = vertexFirst;

    // ── Write ChunkInfo metadata ──────────────────────────────────────────────
    GpuChunkInfo info{};
    info.originWorld = glm::vec4(originWorld, 0.0f);
    info.aabbMin     = glm::vec4(aabbMin,     0.0f);
    info.aabbMax     = glm::vec4(aabbMax,     0.0f);
    info.vertexFirst = vertexFirst;
    info.vertexCount = numVerts;

    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_chunkInfoSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                    (GLintptr)(slot * sizeof(GpuChunkInfo)),
                    sizeof(GpuChunkInfo),
                    &info);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    return slot;
}

// ─────────────────────────────────────────────────────────────────────────────
// free
// ─────────────────────────────────────────────────────────────────────────────

void TerrainGPUBuffer::free(uint32_t slot, uint32_t vertexFirst, uint32_t vertexCount) {
    if (slot == INVALID_SLOT) return;

    // ── Zero out ChunkInfo.vertexCount so the cull shader skips this slot ─────
    // We only need to zero the single field, but zeroing the whole 64-byte struct
    // is equally cheap and ensures no stale data lingers.
    GpuChunkInfo zeroed{};
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_chunkInfoSSBO);
    glBufferSubData(GL_SHADER_STORAGE_BUFFER,
                    (GLintptr)(slot * sizeof(GpuChunkInfo)),
                    sizeof(GpuChunkInfo),
                    &zeroed);
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

    // ── Return the slot to the slot pool ─────────────────────────────────────
    m_freeSlots.push_back(slot);

    // ── Return the vertex block to the pool and merge adjacent free blocks ────
    if (vertexCount == 0) return; // empty chunk had no vertex allocation

    VBlock returned { vertexFirst, vertexCount };

    // Insert the returned block in sorted offset order.
    auto it = m_freeVertexBlocks.begin();
    while (it != m_freeVertexBlocks.end() && it->offset < returned.offset)
        ++it;
    it = m_freeVertexBlocks.insert(it, returned);

    // Merge with the next block if adjacent.
    auto next = std::next(it);
    if (next != m_freeVertexBlocks.end() && it->offset + it->count == next->offset) {
        it->count += next->count;
        m_freeVertexBlocks.erase(next);
    }
    // Merge with the previous block if adjacent.
    if (it != m_freeVertexBlocks.begin()) {
        auto prev = std::prev(it);
        if (prev->offset + prev->count == it->offset) {
            prev->count += it->count;
            m_freeVertexBlocks.erase(it);
        }
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// cull — dispatch the GPU frustum-culling compute shader
// ─────────────────────────────────────────────────────────────────────────────

void TerrainGPUBuffer::cull(const glm::vec3& cameraPos,
                             const std::array<glm::vec4, 6>& frustumPlanes) {
    m_cullShader->use();

    // Bind SSBOs the compute shader reads/writes.
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_chunkInfoSSBO); // readonly
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, m_drawCmdSSBO);   // writeonly

    // Per-frame uniforms.
    glUniform3fv(glGetUniformLocation(m_cullShader->ID, "uCameraPos"), 1, &cameraPos.x);
    glUniform4fv(glGetUniformLocation(m_cullShader->ID, "uFrustumPlanes"), 6,
                 reinterpret_cast<const float*>(frustumPlanes.data()));
    glUniform1ui(glGetUniformLocation(m_cullShader->ID, "uChunkCount"), MAX_CHUNKS);

    // One invocation per slot; 64 invocations per work group.
    const uint32_t groups = (MAX_CHUNKS + 63u) / 64u;
    glDispatchCompute(groups, 1, 1);

    // Wait for the compute shader to finish writing draw commands before the GPU
    // reads them as indirect draw arguments and before shaders read the SSBOs.
    glMemoryBarrier(GL_COMMAND_BARRIER_BIT | GL_SHADER_STORAGE_BARRIER_BIT);
}

// ─────────────────────────────────────────────────────────────────────────────
// bindSSBOs / draw
// ─────────────────────────────────────────────────────────────────────────────

void TerrainGPUBuffer::bindSSBOs() const {
    // Binding 0: ChunkInfo — vertex shaders read chunk origin via gl_BaseInstance
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, m_chunkInfoSSBO);
    // Binding 1: Vertices — vertex shaders read packed data via gl_VertexID
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, m_vertexSSBO);
}

void TerrainGPUBuffer::draw() const {
    glBindVertexArray(emptyVAO);
    bindSSBOs();

    // Bind the draw command buffer so glMultiDrawArraysIndirect reads from GPU memory.
    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, m_drawCmdSSBO);

    // Issue MAX_CHUNKS draw commands in a single call.
    // Slots with instanceCount == 0 (empty or culled) are skipped by the GPU.
    glMultiDrawArraysIndirect(GL_TRIANGLES, nullptr, (GLsizei)MAX_CHUNKS, 0);

    glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
}
