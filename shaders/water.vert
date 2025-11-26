#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 3) in vec3 aNormal;

in vec2 position;

out vec4 clipSpace;
out vec3 worldPos;
out vec3 toCameraVector;
out vec2 textureCoords;

uniform mat4 projection;
uniform mat4 view;
uniform vec3 cameraPos;

uniform float tiling;

void main() {
    vec4 worldPosition = vec4(aPos, 1.0);
    worldPos = worldPosition.xyz;

    // Clip space coordinates for projective texture mapping
    clipSpace = projection * view * worldPosition;
    gl_Position = clipSpace;

//    textureCoords = vec2(position.x/2.0 + 0.5, position.y/2.0 + 0.5) * tiling;
    textureCoords = worldPos.xz * tiling;
    
    // Calculate vectors for lighting and Fresnel
    toCameraVector = cameraPos - worldPosition.xyz;
}
