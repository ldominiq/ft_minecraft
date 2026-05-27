#ifndef PACKED_VERTEX_HPP
#define PACKED_VERTEX_HPP

#include <cstdint>
#include <cmath>

// 8-byte packed terrain vertex (replaces the previous 44-byte / 11-float layout).
// Used by ChunkRenderer for both solid terrain and water meshes. Decoded in
// the vertex shaders via shaders/terrain_vertex_decode.glsl.
//
// Bit layout - chosen so positions can be stored at 1/16-block resolution to
// faithfully represent sub-block geometry (cactus side-inset, etc.).
//
//   v0: posX:9 | posY:13 | posZ:9 | reserved:1
//        (positions are quantized at 1/16 block; range 0-511 / 0-8191 / 0-511
//         which covers a 16x256x16 chunk plus head-room for face boundaries)
//
//   v1: normal:3 | corner:2 | texLayer:10 | skyLight:4 | waterAbove:1 |
//       blockLight:4 (bits 20-23) | reserved:8
//        normal     - face direction index 0..5 (matches mesher's `face` param,
//                     indexes into NORMALS[6] in the GLSL include)
//        corner     - quad corner index 0..3 (UVs reconstructed in the shader)
//        texLayer   - texture array layer (10 bits = 1024 layers)
//        skyLight   - quantized sky-light level 0..15 (matches the engine's
//                     internal 4-bit value; lossless)
//        waterAbove - 1 iff this face's block has a water block directly
//                     above it. Only meaningful for top faces; consumed by
//                     the fragment shader to gate caustics so they don't
//                     appear on dry cave floors that just happen to sit
//                     below sea level.
struct PackedVertex {
    uint32_t v0;
    uint32_t v1;
};
static_assert(sizeof(PackedVertex) == 8, "PackedVertex must be 8 bytes");

namespace packed_vertex {

// Position is stored at 1/16-block resolution. 16.0 chosen so that the
// cactus 1/16-block inset quantizes losslessly.
constexpr float POS_SCALE = 16.0f;

inline uint32_t quantizePos(float px, uint32_t maxValue) {
    long q = std::lroundf(px * POS_SCALE);
    if (q < 0) q = 0;
    if (q > static_cast<long>(maxValue)) q = static_cast<long>(maxValue);
    return static_cast<uint32_t>(q);
}

inline uint32_t quantizeSkyLight(float skyLight01) {
    // Engine sky light is already 0..15 then normalized to 0..1; round-trip back.
    long q = std::lroundf(skyLight01 * 15.0f);
    if (q < 0) q = 0;
    if (q > 15) q = 15;
    return static_cast<uint32_t>(q);
}

// Pack a single vertex. `cornerIdx` is the quad-corner index in [0,3];
// `normalIdx` is the face direction in [0,5] (matches the mesher's `face`).
inline PackedVertex pack(float px, float py, float pz,
                         uint32_t normalIdx, uint32_t cornerIdx,
                         uint32_t texLayer, float skyLight01,
                         bool waterAbove = false,
                         float blockLight01 = 0.0f) {
    const uint32_t qx = quantizePos(px, 0x1FFu);   // 9 bits
    const uint32_t qy = quantizePos(py, 0x1FFFu);  // 13 bits
    const uint32_t qz = quantizePos(pz, 0x1FFu);   // 9 bits

    PackedVertex v;
    v.v0 = (qx & 0x1FFu)
         | ((qy & 0x1FFFu) << 9)
         | ((qz & 0x1FFu)  << 22);
    v.v1 = (normalIdx & 0x7u)
         | ((cornerIdx & 0x3u) << 3)
         | ((texLayer  & 0x3FFu) << 5)
         | ((quantizeSkyLight(skyLight01) & 0xFu) << 15)
         | ((waterAbove ? 1u : 0u) << 19)
         | ((quantizeSkyLight(blockLight01) & 0xFu) << 20);
    return v;
}

// Per-vertex corner index for the 6-vertex quad (two triangles sharing
// the diagonal). Same for every face because the mesher's UV array
// `{(0,0),(1,0),(1,1),(1,1),(0,1),(0,0)}` is shared.
//   v0=corner0 v1=corner1 v2=corner2  v3=corner2 v4=corner3 v5=corner0
inline constexpr uint32_t CORNER_FOR_VERT[6] = { 0, 1, 2, 2, 3, 0 };

} // namespace packed_vertex

#endif // PACKED_VERTEX_HPP
