#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in float aTexLayer;
layout (location = 4) in vec3 aNormal;
layout (location = 5) in float aSkyLight; // Sky-light level (0.0 = dark, 1.0 = full sun)

out VS_OUT {
    vec3 FragPos;
    vec3 FragPosRel;
    vec3 Normal;
    vec2 TexCoord;
    float TexLayer;
    float SkyLight; // Passed to fragment shader for cave darkening
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
// precision quantization at huge distances as before — but the *geometry*
// (gl_Position) is computed from the precise camera-relative path.
uniform vec3 chunkOriginWorld;

// Clipping plane for water reflection/refraction
uniform vec4 clipPlane;

void main()  {
    vec3 worldPos      = chunkOriginWorld + aPos;
    vec3 cameraRelPos  = chunkRel + aPos;

    vs_out.FragPos = worldPos;
    vs_out.FragPosRel = cameraRelPos;
    vs_out.Normal = aNormal;
    vs_out.TexCoord = aTexCoord;
    vs_out.TexLayer = aTexLayer;
    vs_out.SkyLight = aSkyLight;
    gl_Position = projection * viewRot * vec4(cameraRelPos, 1.0);

    // Clip geometry based on plane (used for water reflection/refraction)
    gl_ClipDistance[0] = dot(vec4(worldPos, 1.0), clipPlane);
}
