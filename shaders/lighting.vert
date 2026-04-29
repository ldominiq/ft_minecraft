#version 460 core
#include "terrain_vertex_decode.glsl"
#include "terrain_ssbo.glsl"

// Terrain forward-lighting vertex shader.
//
// Vertex data comes from the vertex SSBO (binding 1) instead of VAO attributes.
// getV0() / getV1() index via gl_VertexID, which is already offset by the
// 'first' field of the MDI draw command — no manual offset needed.
//
// Per-draw chunk info comes from the ChunkInfo SSBO (binding 0) indexed via
// gl_BaseInstance (= chunk slot index set in the draw command).

out VS_OUT {
    vec3 FragPos;       // World-space position (for lighting)
    vec3 FragPosRel;    // Camera-relative position (for projection — keeps precision)
    vec3 Normal;
    vec2 TexCoord;
    float TexLayer;
    float SkyLight;
} vs_out;

uniform mat4 projection;
// View matrix with translation column zeroed — camera sits at the world origin
// in render space.  Keeps the values fed to `projection` small and precise.
uniform mat4 viewRot;
// Camera world position (double→float on CPU). Used to compute the
// camera-relative offset chunkRel = chunkOrigin - cameraPos in the shader.
uniform vec3 cameraPos;
// Clipping plane for water reflection / refraction passes.
uniform vec4 clipPlane;

void main() {
    uint aV0 = getV0();
    uint aV1 = getV1();

    vec3 aPos       = unpackPos(aV0);
    vec2 aTexCoord  = CORNERS[unpackCorner(aV1)];
    float aTexLayer = float(unpackTexLayer(aV1));
    vec3 aNormal    = NORMALS[unpackNormal(aV1)];
    float aSkyLight = unpackSkyLight(aV1);

    // Chunk origin in world space, read from the ChunkInfo SSBO via gl_BaseInstance.
    vec3 chunkOriginWorld = getChunkOrigin();
    // Camera-relative offset (computed on GPU; float precision is fine within render distance).
    vec3 chunkRel = chunkOriginWorld - cameraPos;

    vec3 worldPos     = chunkOriginWorld + aPos;
    vec3 cameraRelPos = chunkRel        + aPos;

    vs_out.FragPos    = worldPos;
    vs_out.FragPosRel = cameraRelPos;
    vs_out.Normal     = aNormal;
    vs_out.TexCoord   = aTexCoord;
    vs_out.TexLayer   = aTexLayer;
    vs_out.SkyLight   = aSkyLight;

    gl_Position = projection * viewRot * vec4(cameraRelPos, 1.0);

    // Clip geometry against a horizontal plane (used by the water reflection pass).
    gl_ClipDistance[0] = dot(vec4(worldPos, 1.0), clipPlane);
}
