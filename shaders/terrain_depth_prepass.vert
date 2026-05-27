#version 460 core
#include "terrain_vertex_decode.glsl"

// Z-prepass for terrain. Reads the same packed VAO as lighting.vert (location
// 0 = v0, location 1 = v1). Emits position + texture coords only - just enough
// to alpha-test leaves so the depth buffer matches the color pass exactly
// (required for glDepthFunc(GL_EQUAL) to work).
layout (location = 0) in uint aV0;
layout (location = 1) in uint aV1;

uniform mat4 projection;
uniform mat4 viewRot;
uniform vec3 chunkRel;
uniform vec3 chunkOriginWorld;
uniform vec4 clipPlane;

out vec2 TexCoord;
flat out float TexLayer;

void main() {
    vec3 aPos       = unpackPos(aV0);
    vec3 cameraRelPos = chunkRel + aPos;
    TexCoord = CORNERS[unpackCorner(aV1)];
    TexLayer = float(unpackTexLayer(aV1));
    gl_Position = projection * viewRot * vec4(cameraRelPos, 1.0);

    // Same clip plane as lighting.vert - the prepass must clip identically
    // to the color pass or GL_EQUAL fails along the water surface.
    gl_ClipDistance[0] = dot(vec4(chunkOriginWorld + aPos, 1.0), clipPlane);
}
