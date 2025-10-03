#version 460 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D diffuseTexture;

void main()
{
    vec3 color = texture(diffuseTexture, TexCoord).rgb;
    FragColor = vec4(color, 1.0);
}
