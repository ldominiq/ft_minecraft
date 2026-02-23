#version 460 core
layout (location = 0) in vec3 aPos;
    
// We do NOT multiply by any matrix here.
// The geometry shader will multiply by the correct lightSpaceMatrix
// for each cascade layer.

void main()
{
    gl_Position = vec4(aPos, 1.0);
}