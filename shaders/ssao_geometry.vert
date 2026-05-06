#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoords;
layout (location = 2) in float aTexLayer;
layout (location = 4) in vec3 aNormal;

out vec3 FragPos;
out vec2 TexCoords;
flat out float TexLayer;
out vec3 Normal;

uniform mat4 view;       // kept for backwards-compat in case any consumer needs world view
uniform mat4 viewRot;    // view with translation zeroed (camera at origin in render space)
uniform mat4 projection;

// Per-chunk: chunkOrigin - cameraPos in double-precision-on-CPU, cast to float.
uniform vec3 chunkRel;

void main()
{
    vec3 cameraRelPos = chunkRel + aPos;
    vec4 viewSpacePos = viewRot * vec4(cameraRelPos, 1.0);
    FragPos = viewSpacePos.xyz;
    TexCoords = aTexCoords;
    TexLayer = aTexLayer;

    Normal = normalize(mat3(viewRot) * aNormal);

    gl_Position = projection * viewSpacePos;
}
