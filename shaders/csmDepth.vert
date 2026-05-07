#version 460 core
#include "terrain_vertex_decode.glsl"

// Packed terrain vertex (8 bytes). See terrain_vertex_decode.glsl.
layout (location = 0) in uint aV0;
layout (location = 1) in uint aV1;

// Multi-pass CSM: each cascade is rendered separately.
// lightSpaceMatrix is updated per-pass on the CPU side.
uniform mat4 lightSpaceMatrix;

// Per-chunk world origin. Mesh vertices are stored in chunk-local coords
// so we reconstruct the world position here before projecting into the
// light's clip space.
uniform vec3 chunkRel;

out vec2 TexCoord;
flat out float TexLayer;

void main()
{
    vec3 aPos = unpackPos(aV0);
    TexCoord = CORNERS[unpackCorner(aV1)];
    TexLayer = float(unpackTexLayer(aV1));
    gl_Position = lightSpaceMatrix * vec4(chunkRel + aPos, 1.0);
}
