#version 460 core
#include "terrain_vertex_decode.glsl"

// Debug "gradient by Y" shader. Uses the same packed terrain VAO so it can
// be swapped in for textureShader at the activeShader pointer site.
layout (location = 0) in uint aV0;
layout (location = 1) in uint aV1;

out float blockY;

uniform mat4 projection;
uniform mat4 viewRot;
uniform vec3 chunkRel;
uniform vec3 chunkOriginWorld;

// Clipping plane for water reflection/refraction
uniform vec4 clipPlane;

void main() {
    vec3 aPos = unpackPos(aV0);
    vec3 cameraRelPos = chunkRel + aPos;
    vec3 worldPos = chunkOriginWorld + aPos;

    gl_Position = projection * viewRot * vec4(cameraRelPos, 1.0);

    // blockY drove a hue gradient in the fragment shader. The redundant
    // gradientY attribute is gone; just use the decoded Y position.
    blockY = aPos.y;

    // Clip geometry based on plane (used for water reflection/refraction)
    gl_ClipDistance[0] = dot(vec4(worldPos, 1.0), clipPlane);
}
