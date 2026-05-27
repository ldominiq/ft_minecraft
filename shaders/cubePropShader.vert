#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in float aTexLayer;
// Per-vertex sky-light, normalized 0..1. All 36 verts of one dropped item
// share the same value (the host samples once per entity at its position).
// Per-vertex rather than uniform because every item in the world is packed
// into a single draw call.
layout (location = 3) in float aSkyLight;
// Per-vertex baked torch block-light, 0..1. Separate from aSkyLight so an
// item can be both sky-lit and torch-lit (no one-float encoding tricks).
layout (location = 4) in float aBlockLight;

out vec2 TexCoord;
flat out float TexLayer;
// Camera-relative world position + face normal for entity_lighting.glsl.
out vec3 vFragPosRel;
flat out vec3 vNormal;
flat out float vSkyLight;
flat out float vBlockLight;

// Translation-free view (camera at origin of render space). aPos is already
// camera-relative, so we never want the full view (its translation would
// double-subtract). Matches the name lighting.vert uses, which is also what
// Lighting::uploadCSMUniforms sets - no collision.
uniform mat4 viewRot;
uniform mat4 projection;

// Matches faceNormals[] in blockRenderingHelperFunctions.hpp. ItemPropEntity
// emits faces in order 0..5 with 6 vertices each, so face = vertexID/6 mod 6.
const vec3 FACE_NORMALS[6] = vec3[6](
    vec3( 0.0, 0.0,  1.0),
    vec3( 0.0, 0.0, -1.0),
    vec3( 0.0, 1.0,  0.0),
    vec3( 0.0,-1.0,  0.0),
    vec3( 1.0, 0.0,  0.0),
    vec3(-1.0, 0.0,  0.0)
);

void main()
{
    gl_Position = projection * viewRot * vec4(aPos, 1.0);
    TexCoord = aTexCoord;
    TexLayer = aTexLayer;

    // aPos is already camera-relative (createMesh subtracts eyePos).
    vFragPosRel = aPos;
    int face = (gl_VertexID / 6) % 6;
    vNormal = FACE_NORMALS[face];
    vSkyLight = aSkyLight;
    vBlockLight = aBlockLight;
}
