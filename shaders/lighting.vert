#version 460 core
#include "terrain_vertex_decode.glsl"

// Packed terrain vertex (8 bytes). See terrain_vertex_decode.glsl.
layout (location = 0) in uint aV0;
layout (location = 1) in uint aV1;

out VS_OUT {
    vec3 FragPos;
    vec3 FragPosRel;
    vec3 Normal;
    vec2 TexCoord;
    float TexLayer;
    float SkyLight; // Passed to fragment shader for cave darkening
    float WaterAbove; // 1.0 iff a water block sits directly above (top faces)
    float BlockLight; // Emissive torch light, 0..1 (not sky-modulated)
} vs_out;

uniform mat4 projection;
// View matrix with translation column zeroed (camera at origin in render
// space). Used together with chunkRel below to keep the inputs to the
// projection small even when the player is far from world origin.
uniform mat4 viewRot;
// Per-chunk: chunkOrigin - cameraPos, computed CPU-side in double precision.
uniform vec3 chunkRel;
// Per-chunk: chunkOrigin in world space (float). Used only to reconstruct
// world-space FragPos for lighting / shadows / fog. Suffers the same
// precision quantization at huge distances as before - but the *geometry*
// (gl_Position) is computed from the precise camera-relative path.
uniform vec3 chunkOriginWorld;

// Clipping plane for water reflection/refraction
uniform vec4 clipPlane;

void main()  {
    vec3 aPos      = unpackPos(aV0);
    vec2 aTexCoord = CORNERS[unpackCorner(aV1)];
    float aTexLayer= float(unpackTexLayer(aV1));
    vec3 aNormal   = NORMALS[unpackNormal(aV1)];
    float aSkyLight= unpackSkyLight(aV1);
    float aWaterAbove = unpackWaterAbove(aV1);
    float aBlockLight = unpackBlockLight(aV1);

    vec3 worldPos      = chunkOriginWorld + aPos;
    vec3 cameraRelPos  = chunkRel + aPos;

    vs_out.FragPos = worldPos;
    vs_out.FragPosRel = cameraRelPos;
    vs_out.Normal = aNormal;
    vs_out.TexCoord = aTexCoord;
    vs_out.TexLayer = aTexLayer;
    vs_out.SkyLight = aSkyLight;
    vs_out.WaterAbove = aWaterAbove;
    vs_out.BlockLight = aBlockLight;
    gl_Position = projection * viewRot * vec4(cameraRelPos, 1.0);

    // Clip geometry based on plane (used for water reflection/refraction)
    gl_ClipDistance[0] = dot(vec4(worldPos, 1.0), clipPlane);
}
