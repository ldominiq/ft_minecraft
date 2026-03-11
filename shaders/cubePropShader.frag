#version 460 core

in vec2 TexCoord;
flat in float TexLayer;
out vec4 FragColor;

uniform sampler2DArray blockTextures;

void main()
{
    vec3 color = texture(blockTextures, vec3(TexCoord, TexLayer)).rgb;
    FragColor = vec4(color, 1.0);
}
