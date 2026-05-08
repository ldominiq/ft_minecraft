#version 460 core
#include "terrain_vertex_decode.glsl"

// Placed-water mesh uses the same packed vertex format as terrain/ocean
// water. We only consume position; the rest is decoded but unused.
layout (location = 0) in uint aV0;
layout (location = 1) in uint aV1;

out vec4 clipSpace;
out vec3 toCameraVector;
out vec2 textureCoords;

uniform mat4 projection;
uniform mat4 viewRot;
uniform vec3 chunkRel;

// (eye.xz - anchor.xz) where anchor is snapped to a multiple of the dudv
// repeat period (1/tiling), computed in double on the CPU. Keeps the
// texture coordinate magnitude small (< period) so adding moveFactor in the
// fragment shader is not quantized away at large world coordinates.
uniform vec2 texAnchor;
uniform float tiling;

void main() {
    vec3 aPos = unpackPos(aV0);
    vec3 cameraRelPos = chunkRel + aPos;

    clipSpace = projection * viewRot * vec4(cameraRelPos, 1.0);
    gl_Position = clipSpace;

    textureCoords = (cameraRelPos.xz + texAnchor) * tiling;

    // Vector from fragment toward the camera, in the camera-relative,
    // pre-rotation frame. Its components are aligned with world axes (the
    // camera's translation is the only thing stripped), which is what we
    // want for sun-direction dot products and skyLUT lookups.
    toCameraVector = -cameraRelPos;
}
