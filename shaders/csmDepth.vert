#version 460 core
#include "terrain_vertex_decode.glsl"
#include "terrain_ssbo.glsl"

// CSM shadow-map depth pass.
//
// Vertex data from SSBO binding 1 (gl_VertexID → packed vertex).
// Per-chunk origin from SSBO binding 0 (gl_BaseInstance → slot index).
// Called via glDrawArraysInstancedBaseInstance so gl_BaseInstance = chunk slot index.

// Light-space transform for the current cascade — updated once per cascade on CPU.
uniform mat4 lightSpaceMatrix;
// Camera (eye) world position — used to reconstruct the camera-relative position
// that the light-space matrix expects (same convention as the main render pass).
uniform vec3 cameraPos;

out vec2 TexCoord;
flat out float TexLayer;

void main() {
    vec3 aPos = unpackPos(getV0());
    TexCoord  = CORNERS[unpackCorner(getV1())];
    TexLayer  = float(unpackTexLayer(getV1()));

    // Camera-relative position (matches the convention in lightSpaceMatrix).
    vec3 chunkRel     = getChunkOrigin() - cameraPos;
    vec3 cameraRelPos = chunkRel + aPos;

    gl_Position = lightSpaceMatrix * vec4(cameraRelPos, 1.0);
}
