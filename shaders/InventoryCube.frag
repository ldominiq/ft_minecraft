#version 460 core

in vec2 vUV;
flat in float TexLayer;
out vec4 FragColor;

uniform sampler2DArray blockTextures;

void main()
{
    vec4 tex = texture(blockTextures, vec3(vUV, TexLayer));

    // Alpha discard for transparent blocks
    if (tex.a < 0.1)
        discard;

    FragColor = tex;
}
