#version 460 core
#include "terrain_vertex_decode.glsl"
#include "terrain_ssbo.glsl"

// Z-prepass for terrain.
//
// Reads the same packed vertex data as lighting.vert but only emits position
// and texture coordinates — just enough to alpha-test leaves so the depth
// buffer matches the colour pass exactly (required for glDepthFunc(GL_EQUAL)).
//
// Vertex data from SSBO binding 1, per-chunk info from SSBO binding 0.
// Called via the same MDI path as the main forward pass.

uniform mat4 projection;
uniform mat4 viewRot;
uniform vec3 cameraPos;
uniform vec4 clipPlane;

out vec2 TexCoord;
flat out float TexLayer;

void main() {
    uint aV0 = getV0();
    uint aV1 = getV1();

    vec3 aPos           = unpackPos(aV0);
    vec3 chunkOrigin    = getChunkOrigin();
    vec3 chunkRel       = chunkOrigin - cameraPos;
    vec3 cameraRelPos   = chunkRel + aPos;

    TexCoord = CORNERS[unpackCorner(aV1)];
    TexLayer = float(unpackTexLayer(aV1));

    gl_Position = projection * viewRot * vec4(cameraRelPos, 1.0);

    // Must clip identically to the colour pass or glDepthFunc(GL_EQUAL) fails
    // along the water surface.
    gl_ClipDistance[0] = dot(vec4(chunkOrigin + aPos, 1.0), clipPlane);
}
