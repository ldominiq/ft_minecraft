#version 460 core

in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D atlas;

void main()
{
    vec3 color = texture(atlas, TexCoord).rgb;
    FragColor = vec4(color, 1.0);
}
