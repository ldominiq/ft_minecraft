#version 460 core

layout (location = 0) in vec2 position;

out vec2 textureCoords;

uniform mat4 transformationMatrix;
uniform bool flipY;

void main(void) {
    gl_Position = transformationMatrix * vec4(position, 0.0, 1.0);
    float ty = (position.y + 1.0) / 2.0;
    if (flipY)
        textureCoords = vec2((position.x + 1.0) / 2.0, ty);         // no flip - correct for FBOs
    else
        textureCoords = vec2((position.x + 1.0) / 2.0, 1.0 - ty);   // flip - correct for loaded textures
}