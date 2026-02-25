#version 460 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoords;

out vec2 TexCoords;

uniform vec2 offset;   // bottom-left corner in NDC
uniform vec2 scale;    // size in NDC

void main()
{
    // Map [0,1] quad to the desired screen rectangle
    vec2 pos = aPos * scale + offset;
    gl_Position = vec4(pos, 0.0, 1.0);
    TexCoords = aTexCoords;
}
