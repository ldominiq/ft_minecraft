// terrain_ssbo.glsl
// ─────────────────────────────────────────────────────────────────────────────
// Declares the SSBOs used by all terrain vertex shaders and provides three
// helper functions that replace the old per-vertex attributes and per-draw
// uniforms.
//
// Include this file in every terrain shader (lighting, depth prepass, SSAO,
// CSM shadow).  Do NOT include it in water.vert — water still uses VAO attributes.
//
// Usage in main():
//   uint v0 = getV0();            // packed bits: posX | posY | posZ
//   uint v1 = getV1();            // packed bits: normal | corner | texLayer | skyLight
//   vec3 o  = getChunkOrigin();   // chunk world-space origin
//   vec3 chunkRel = o - cameraPos;   // compute camera-relative offset here
// ─────────────────────────────────────────────────────────────────────────────

// ── SSBO 0: Per-chunk metadata ────────────────────────────────────────────────
// Matches GpuChunkInfo in TerrainGPUBuffer.hpp (std430, 64 bytes per slot):
//   slot*4 + 0  →  vec4 originWorld   (.xyz = world-space chunk origin)
//   slot*4 + 1  →  vec4 aabbMin       (.xyz = AABB min, not needed in vertex shaders)
//   slot*4 + 2  →  vec4 aabbMax       (.xyz = AABB max, not needed in vertex shaders)
//   slot*4 + 3  →  uvec4 { vertexFirst, vertexCount, pad, pad } (not accessed here)
//
// We use a flat vec4 array rather than a typed struct to avoid GLSL-to-C++
// struct padding surprises — every field is accessed at a manually-computed index.
#define CHUNK_INFO_STRIDE 4  // Number of vec4s per GpuChunkInfo entry (64 / 16 = 4)

layout(std430, binding = 0) readonly buffer ChunkInfoBuf {
    vec4 gChunkInfoData[];
};

// ── SSBO 1: Packed vertex data ────────────────────────────────────────────────
// All terrain chunk vertices concatenated.  Each element is a uvec2 storing the
// two uint32 words of PackedVertex (v0 = posX|posY|posZ, v1 = normal|corner|...).
layout(std430, binding = 1) readonly buffer VertexBuf {
    uvec2 gVertices[];
};

// ── Vertex fetch helpers ──────────────────────────────────────────────────────
// gl_VertexID already includes the per-draw 'first' offset from the draw command
// (set by glMultiDrawArraysIndirect or glDrawArraysInstancedBaseInstance), so it
// directly indexes the correct chunk's range inside gVertices.

uint getV0() { return gVertices[gl_VertexID].x; }
uint getV1() { return gVertices[gl_VertexID].y; }

// ── Per-draw chunk origin ─────────────────────────────────────────────────────
// gl_BaseInstance is set to the chunk slot index:
//   - In the MDI forward pass:  baseInstance field of GpuDrawCommand (= slot index)
//   - In shadow/depth passes:   baseInstance arg of glDrawArraysInstancedBaseInstance
// This lets every shader know which chunk it's drawing without any per-draw uniforms.
vec3 getChunkOrigin() {
    // originWorld is the first vec4 of this slot's GpuChunkInfo entry.
    return gChunkInfoData[gl_BaseInstance * CHUNK_INFO_STRIDE].xyz;
}
