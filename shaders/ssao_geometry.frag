#version 460 core

layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gNormal;

in vec2 TexCoords;
flat in float TexLayer;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2DArray blockTextures;
uniform bool useAlphaTest;

void main() {
    // Alpha test gated by leaf-render mode (Fast = off, Fancy/Smart = on).
    // Skipping when off keeps GBuffer fully populated for opaque-leaf pixels.
    if (useAlphaTest) {
        vec4 texColor = texture(blockTextures, vec3(TexCoords, TexLayer));
        if (texColor.a < 0.1)
            discard;
    }

    // store the fragment position vector in the first gbuffer texture
    gPosition = vec4(FragPos, 1.0);
    // also store the per-fragment normals into the gbuffer
    gNormal = vec4(normalize(Normal), 0.0);
}