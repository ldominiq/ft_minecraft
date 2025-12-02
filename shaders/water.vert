#version 460 core
layout (location = 0) in vec3 aPos;

out vec4 clipSpace;
out vec3 toCameraVector;
out vec2 textureCoords;
out vec3 fromLightVector;
out vec3 lightPos;

uniform mat4 projection;
uniform mat4 view;
uniform vec3 cameraPos;
uniform vec3 lightPosition;

uniform float tiling;

void main() {
    vec4 worldPosition = vec4(aPos, 1.0);

    // Clip space coordinates for projective texture mapping
    clipSpace = projection * view * worldPosition;
    gl_Position = clipSpace;

    textureCoords = worldPosition.xz * tiling;
    
    // Calculate vectors for lighting and Fresnel
    toCameraVector = cameraPos - worldPosition.xyz;

    fromLightVector = worldPosition.xyz - lightPosition;

    lightPos = lightPosition;
}
