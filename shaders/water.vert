#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 3) in vec3 aNormal;

out vec4 clipSpace;
out vec3 worldPos;
out vec3 toCameraVector;

uniform mat4 projection;
uniform mat4 view;
uniform vec3 cameraPos;

void main() {
    vec4 worldPosition = vec4(aPos, 1.0);
    worldPos = worldPosition.xyz;

    // Clip space coordinates for projective texture mapping
    clipSpace = projection * view * worldPosition;
    gl_Position = clipSpace;
    
    // Calculate vectors for lighting and Fresnel
    toCameraVector = cameraPos - worldPosition.xyz;
}
