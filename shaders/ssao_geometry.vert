#version 460 core
#include "terrain_vertex_decode.glsl"
#include "terrain_ssbo.glsl"

// SSAO geometry pass — writes view-space position and normal to the G-buffer.
//
// Vertex data from SSBO binding 1, per-chunk origin from SSBO binding 0.
// Called via the same MDI path as the main forward pass.

out vec3 FragPos;    // View-space position
out vec2 TexCoords;
flat out float TexLayer;
out vec3 Normal;     // View-space normal

uniform mat4 view;       // Full view matrix (kept for any consumer that needs it)
uniform mat4 viewRot;    // View with translation zeroed (camera at origin in render space)
uniform mat4 projection;
uniform vec3 cameraPos;  // Camera world position

void main() {
    uint aV0 = getV0();
    uint aV1 = getV1();

    vec3 aPos     = unpackPos(aV0);
    vec3 aNormal  = NORMALS[unpackNormal(aV1)];

    vec3 chunkRel     = getChunkOrigin() - cameraPos;
    vec3 cameraRelPos = chunkRel + aPos;

    // Transform to view space for the SSAO kernel.
    vec4 viewSpacePos = viewRot * vec4(cameraRelPos, 1.0);
    FragPos    = viewSpacePos.xyz;
    TexCoords  = CORNERS[unpackCorner(aV1)];
    TexLayer   = float(unpackTexLayer(aV1));
    Normal     = normalize(mat3(viewRot) * aNormal);

    gl_Position = projection * viewSpacePos;
}
