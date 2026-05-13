// terrain_vertex_decode.glsl
// Decoder for the 8-byte PackedVertex format used by terrain + water meshes.
// Included by every vertex shader that consumes a chunk VAO.
//
// Bit layout (must match include/client/PackedVertex.hpp):
//   v0: posX:9 | posY:13 | posZ:9 | reserved:1
//        — positions are 1/16-block fixed point (multiply by 1/16 to recover floats)
//   v1: normal:3 | corner:2 | texLayer:10 | skyLight:4 | waterAbove:1 | reserved:12

// Face direction lookup. Matches ChunkRenderer's `face` parameter ordering:
//   0 = front (Z+), 1 = back (Z-), 2 = top (Y+), 3 = bottom (Y-),
//   4 = right (X+), 5 = left  (X-)
const vec3 NORMALS[6] = vec3[6](
    vec3( 0.0,  0.0,  1.0),
    vec3( 0.0,  0.0, -1.0),
    vec3( 0.0,  1.0,  0.0),
    vec3( 0.0, -1.0,  0.0),
    vec3( 1.0,  0.0,  0.0),
    vec3(-1.0,  0.0,  0.0)
);

// Quad corner UVs. Vertex shader picks one via the per-vert cornerIdx that
// the mesher writes (sequence per face: 0,1,2, 2,3,0).
const vec2 CORNERS[4] = vec2[4](
    vec2(0.0, 0.0),
    vec2(1.0, 0.0),
    vec2(1.0, 1.0),
    vec2(0.0, 1.0)
);

vec3 unpackPos(uint v0) {
    // 1/16 block fixed point — divide to recover float position.
    return vec3(
        float( v0        & 0x1FFu),
        float((v0 >>  9) & 0x1FFFu),
        float((v0 >> 22) & 0x1FFu)
    ) * (1.0 / 16.0);
}

int   unpackNormal  (uint v1) { return int( v1        & 0x7u);   }
int   unpackCorner  (uint v1) { return int((v1 >> 3)  & 0x3u);   }
int   unpackTexLayer(uint v1) { return int((v1 >> 5)  & 0x3FFu); }
float unpackSkyLight(uint v1) { return float((v1 >> 15) & 0xFu) / 15.0; }
// 1.0 if this face's block has water directly above (top faces only);
// 0.0 otherwise. Used by the lighting fragment shader to gate caustics.
float unpackWaterAbove(uint v1) { return float((v1 >> 19) & 0x1u); }
