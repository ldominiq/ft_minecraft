#version 460 core
layout (location = 0) in vec3 aPos;

out vec4 clipSpace;
out vec3 toCameraVector;
out vec2 textureCoords;

uniform mat4 projection;
uniform mat4 viewRot;
uniform vec3 chunkRel;

// (eye.xz - anchor.xz) where anchor is snapped to a multiple of the dudv
// repeat period (1/tiling), computed in double on the CPU. This keeps the
// texture coordinate magnitude small (< period) so that adding moveFactor
// in the fragment shader is not quantized away at large world coordinates.
uniform vec2 texAnchor;
uniform float tiling;

void main() {
    vec3 cameraRelPos = chunkRel + aPos;

    // Clip space coordinates for projective texture mapping
    clipSpace = projection * viewRot * vec4(cameraRelPos, 1.0);
    gl_Position = clipSpace;

    // Equivalent to worldPos.xz * tiling modulo the texture period, but
    // computed from camera-relative coordinates so it stays precise far
    // from the world origin.
    textureCoords = (cameraRelPos.xz + texAnchor) * tiling;

    // Calculate vectors for lighting and Fresnel
    toCameraVector = -cameraRelPos;
}
