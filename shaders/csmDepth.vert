#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;

// Multi-pass CSM: each cascade is rendered separately.
// lightSpaceMatrix is updated per-pass on the CPU side.
uniform mat4 lightSpaceMatrix;

out vec2 TexCoord;

void main()
{
    TexCoord = aTexCoord;
    gl_Position = lightSpaceMatrix * vec4(aPos, 1.0);
}