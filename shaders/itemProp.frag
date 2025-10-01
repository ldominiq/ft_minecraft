#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D blockTexture;

void main()
{
    vec4 texColor = texture(blockTexture, TexCoord);

    if (texColor.a < 0.1) discard; // handle transparent pixels

    FragColor = texColor;
}
