#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in float aTexLayer;

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
    TexCoord = aTexCoord;
    TexLayer = aTexLayer;
    gl_Position = lightSpaceMatrix * vec4(chunkRel + aPos, 1.0);
}
