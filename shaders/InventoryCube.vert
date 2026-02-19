#version 460 core

layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aUV;

out vec2 vUV;

uniform vec2 uScreenSize; // window size in pixels

void main()
{
    // Convert from screen-space to NDC
    vec2 ndc = (aPos / uScreenSize) * 2.0 - 1.0;

    gl_Position = vec4(ndc, 0.0, 1.0);
    vUV = aUV;
}
