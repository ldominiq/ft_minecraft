#version 460 core
#include "terrain_vertex_decode.glsl"

// Water meshes use the same packed vertex format as terrain. Water shader
// only needs position; the other fields are decoded but unused.
layout (location = 0) in uint aV0;
layout (location = 1) in uint aV1;

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

// Gerstner wave inputs. waveAnchor is (eye.xz - waveAnchor) where waveAnchor
// is snapped to a 1024-unit grid; adding it to cameraRelPos.xz reconstructs a
// world-space xz that is stable across chunks but bounded for f32 precision.
uniform vec2 waveAnchor;
uniform float waveTime;

// Sum of two Gerstner waves. Returns vertical displacement only (we don't
// circular-displace the xz, since the mesh is a 1×1 grid per quad - pure y
// displacement is enough to give silhouette wobble and parallax).
//

float gerstnerY(vec2 worldXZ) {
    // Wave 1 - broad, slow.
    const vec2  dir1 = vec2( 0.7071,  0.7071);
    const float len1 = 16.0;
    const float amp1 = 0.05; // wave amplitude (max vertical displacement)
    const float k1   = 6.28318530718 / len1;
    float phase1     = k1 * dot(dir1, worldXZ) - waveTime * 1.5;

    // Wave 2 - small, fast, crossing direction.
    const vec2  dir2 = vec2(-0.5,     0.866);
    const float len2 = 7.0;
    const float amp2 = 0.024; // smaller amplitude for the second wave
    const float k2   = 6.28318530718 / len2;
    float phase2     = k2 * dot(dir2, worldXZ) - waveTime * 2.3;

    return amp1 * cos(phase1) + amp2 * cos(phase2);
}

void main() {
    vec3 aPos = unpackPos(aV0);
    vec3 cameraRelPos = chunkRel + aPos;

    // Displace water-surface vertices. A vertex sits on the surface when
    // either:
    //   - its face normal is +Y (top face - all four corners are surface verts), or
    //   - it's a side face vertex on the upper edge of the block.
    // Mesher emits 6 verts per face with cornerIdx cycling 0,1,2,2,3,0; for
    // side faces, corners 2 and 3 are the upper edge (top-right / top-left
    // of the quad UV), corresponding to y == blockY + 1.
    int normalIdx = unpackNormal(aV1);   // 2 = top, 3 = bottom, others = sides
    int cornerIdx = unpackCorner(aV1);
    bool isTopFace     = (normalIdx == 2);
    bool isBottomFace  = (normalIdx == 3);
    bool isUpperEdge   = (cornerIdx == 2 || cornerIdx == 3);
    bool displace      = isTopFace || (!isBottomFace && isUpperEdge);

    if (displace) {
        vec2 worldXZ = cameraRelPos.xz + waveAnchor;
        cameraRelPos.y += gerstnerY(worldXZ);
    }

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
