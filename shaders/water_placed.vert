#version 460 core
#include "terrain_vertex_decode.glsl"

// Placed-water mesh uses the same packed vertex format as terrain/ocean
// water. We only consume position; the rest is decoded but unused.
layout (location = 0) in uint aV0;
layout (location = 1) in uint aV1;

out vec4 clipSpace;
out vec3 toCameraVector;
out vec2 textureCoords;
// Pass the face index so the fragment can build the correct TBN and
// interpret the dudv/normal map as tangent-space rather than world-space.
// Without this, side faces inherit the +Y-up assumption baked into the
// top-face path and look stretched / wrongly lit.
flat out int faceNormalIdx;

uniform mat4 projection;
uniform mat4 viewRot;
uniform vec3 chunkRel;

// (eye.xz - anchor.xz) where anchor is snapped to a multiple of the dudv
// repeat period (1/tiling), computed in double on the CPU. Keeps the
// texture coordinate magnitude small (< period) so adding moveFactor in the
// fragment shader is not quantized away at large world coordinates.
uniform vec2 texAnchor;
// Vertical companion to texAnchor.xz, used by side-face UVs so the texture
// stays world-space-stable when the eye moves up or down. Anchored to a
// multiple of the dudv repeat period on the CPU - same trick as XZ.
uniform float texAnchorY;
uniform float tiling;

// Gerstner wave inputs
uniform vec2 waveAnchor;
uniform float waveTime;

// Smaller-amplitude Gerstner 
float gerstnerY(vec2 worldXZ) {
    const vec2  dir1 = vec2( 0.7071,  0.7071);
    const float len1 = 16.0;
    const float amp1 = 0.025;
    const float k1   = 6.28318530718 / len1;
    float phase1     = k1 * dot(dir1, worldXZ) - waveTime * 1.5;

    const vec2  dir2 = vec2(-0.5,     0.866);
    const float len2 = 7.0;
    const float amp2 = 0.012;
    const float k2   = 6.28318530718 / len2;
    float phase2     = k2 * dot(dir2, worldXZ) - waveTime * 2.3;

    return amp1 * cos(phase1) + amp2 * cos(phase2);
}

void main() {
    vec3 aPos = unpackPos(aV0);
    vec3 cameraRelPos = chunkRel + aPos;

    int normalIdx = unpackNormal(aV1);
    int cornerIdx = unpackCorner(aV1);
    bool isTopFace    = (normalIdx == 2);
    bool isBottomFace = (normalIdx == 3);
    bool isUpperEdge  = (cornerIdx == 2 || cornerIdx == 3);
    bool displace     = isTopFace || (!isBottomFace && isUpperEdge);

    if (displace) {
        vec2 worldXZ = cameraRelPos.xz + waveAnchor;
        cameraRelPos.y += gerstnerY(worldXZ);
    }

    clipSpace = projection * viewRot * vec4(cameraRelPos, 1.0);
    gl_Position = clipSpace;

    // Per-face UV projection. Side faces project onto (horizontal axis, Y)
    // so the dudv texture doesn't smear vertically - using only XZ would
    // give a constant UV across a side face's vertical extent, which is
    // exactly what made the sides look stretched. Y has no anchor (chunk
    // height is bounded, so it stays in f32 range) and feeds V directly.
    vec2 uvWorld;
    if (normalIdx == 2 || normalIdx == 3) {
        // Top / bottom: project XZ. texAnchor anchors both axes.
        uvWorld = cameraRelPos.xz + texAnchor;
    } else if (normalIdx == 0 || normalIdx == 1) {
        // Z+ / Z-: project XY. X uses texAnchor.x, Y uses texAnchorY.
        uvWorld = vec2(cameraRelPos.x + texAnchor.x, cameraRelPos.y + texAnchorY);
    } else {
        // X+ / X-: project ZY. Z uses texAnchor.y, Y uses texAnchorY.
        uvWorld = vec2(cameraRelPos.z + texAnchor.y, cameraRelPos.y + texAnchorY);
    }
    textureCoords = uvWorld * tiling;
    faceNormalIdx = normalIdx;

    // Vector from fragment toward the camera, in the camera-relative,
    // pre-rotation frame. Its components are aligned with world axes (the
    // camera's translation is the only thing stripped), which is what we
    // want for sun-direction dot products and skyLUT lookups.
    toCameraVector = -cameraRelPos;
}
