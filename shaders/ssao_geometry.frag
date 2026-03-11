#version 460 core

layout (location = 0) out vec4 gPosition;
layout (location = 1) out vec4 gNormal;

in vec2 TexCoords;
in vec3 FragPos;
in vec3 Normal;

uniform sampler2D atlas;

void main() {
    // Discard fully transparent fragments
    vec4 texColor = texture(atlas, TexCoords);
    if (texColor.a < 0.1)
        discard;

    // store the fragment position vector in the first gbuffer texture
    gPosition = vec4(FragPos, 1.0);
    // also store the per-fragment normals into the gbuffer
    gNormal = vec4(normalize(Normal), 0.0);
}