#version 460 core
out vec4 FragColor;

uniform vec4 uColor;   // RGBA (last component = alpha)

void main()
{
    FragColor = uColor;
}
