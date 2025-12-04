#version 460 core

uniform vec3 uColor;   // uniform color for the whole object

out vec4 FragColor;

void main()
{
    FragColor = vec4(uColor, 1.0); // apply uniform color
}
