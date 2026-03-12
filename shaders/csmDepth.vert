#version 460 core
layout (location = 0) in vec3 aPos;

// Multi-pass CSM: each cascade is rendered separately.
// lightSpaceMatrix is updated per-pass on the CPU side.
uniform mat4 lightSpaceMatrix;

void main()
{
    gl_Position = lightSpaceMatrix * vec4(aPos, 1.0);
}