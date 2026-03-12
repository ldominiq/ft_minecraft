#version 460 core
layout (location = 0) in vec2 position;
layout (location = 1) in vec2 texCoords;

out vec2 TexCoords;

uniform vec2 offset; // NDC offset to reposition the quad

void main()
{
    gl_Position = vec4(position + offset, 0.0f, 1.0f);
    TexCoords = texCoords;
}
