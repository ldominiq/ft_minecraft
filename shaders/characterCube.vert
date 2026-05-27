#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in float aFaceIndex;
layout (location = 2) in vec2 aFaceCorner;

uniform mat4 uProjection;
uniform mat4 uViewRot;
uniform mat4 uModelRel;
// Per-part face UV rects in normalized skin-auth space (V=0 is top of PNG).
// xy = (u_left, v_top), zw = (u_right, v_bottom). Indexed 0..5.
uniform vec4 uFaceUVs[6];

out vec2 vTex;
// Camera-relative world position + world-space normal for entity_lighting.glsl.
out vec3 vFragPosRel;
out vec3 vNormal;

// Matches the character cube face indexing in src/client/characters/cube.cpp -
// NOT the block face order in blockRenderingHelperFunctions.hpp. Character rig
// axes: +X = character front (face), +Y = up, +Z = character left.
const vec3 FACE_NORMALS[6] = vec3[6](
    vec3( 1.0, 0.0,  0.0),  // 0: +X - character front (face)
    vec3(-1.0, 0.0,  0.0),  // 1: -X - character back
    vec3( 0.0, 1.0,  0.0),  // 2: +Y - top
    vec3( 0.0,-1.0,  0.0),  // 3: -Y - bottom
    vec3( 0.0, 0.0, -1.0),  // 4: -Z - character right
    vec3( 0.0, 0.0,  1.0)   // 5: +Z - character left
);

void main()
{
    int face = int(aFaceIndex + 0.5);
    vec4 rect = uFaceUVs[face];
    vTex = mix(rect.xy, rect.zw, aFaceCorner);

    // uModelRel is the camera-relative body-part transform (rotation+translate,
    // no nonuniform scale). Pass world position relative to camera + world normal
    // to the fragment so CSM + point lights resolve correctly.
    vec4 worldRel = uModelRel * vec4(aPos, 1.0);
    vFragPosRel = worldRel.xyz;
    vNormal = normalize(mat3(uModelRel) * FACE_NORMALS[face]);

    gl_Position = uProjection * uViewRot * worldRel;
}
