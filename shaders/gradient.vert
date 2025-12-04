#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 2) in float aY;

out float blockY;

uniform mat4 view;
uniform mat4 projection;

// Clipping plane for water reflection/refraction
uniform vec4 clipPlane;

void main() {
    vec4 worldPosition = vec4(aPos, 1.0);
    gl_Position = projection * view * worldPosition;

    blockY = aY;
    
    // Clip geometry based on plane (used for water reflection/refraction)
    gl_ClipDistance[0] = dot(worldPosition, clipPlane);
}
