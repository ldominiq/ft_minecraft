#version 460 core
#include "terrain_vertex_decode.glsl"

// Packed terrain vertex (8 bytes). See terrain_vertex_decode.glsl.
layout (location = 0) in uint aV0;
layout (location = 1) in uint aV1;

out vec3 FragPos;
out vec2 TexCoords;
flat out float TexLayer;
out vec3 Normal;

uniform mat4 view;       // kept for backwards-compat in case any consumer needs world view
uniform mat4 viewRot;    // view with translation zeroed (camera at origin in render space)
uniform mat4 projection;

// Per-chunk: chunkOrigin - cameraPos in double-precision-on-CPU, cast to float.
uniform vec3 chunkRel;

void main()
{
    vec3 aPos       = unpackPos(aV0);
    vec3 aNormal    = NORMALS[unpackNormal(aV1)];

    vec3 cameraRelPos = chunkRel + aPos;
    vec4 viewSpacePos = viewRot * vec4(cameraRelPos, 1.0);
    FragPos = viewSpacePos.xyz;
    TexCoords = CORNERS[unpackCorner(aV1)];
    TexLayer = float(unpackTexLayer(aV1));

    Normal = normalize(mat3(viewRot) * aNormal);

    gl_Position = projection * viewSpacePos;
}
