#version 460 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord;
layout (location = 3) in vec3 aNormal;

out vec2 TexCoord;
out vec4 clipSpace;
out vec3 toCameraVector;
out vec3 fromLightVector;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 lightSpaceMatrix;
uniform vec3 cameraPos;
uniform vec3 lightPos;

// Moving factor for DuDv distortion (updated per frame based on time)
uniform float moveFactor;

void main() {
    vec4 worldPosition = vec4(aPos, 1.0);
    
    // Clip space coordinates for projective texture mapping
    clipSpace = projection * view * worldPosition;
    gl_Position = clipSpace;
    
    // Pass through texture coordinates
    TexCoord = aTexCoord;
    
    // Calculate vectors for lighting and Fresnel
    toCameraVector = cameraPos - worldPosition.xyz;
    fromLightVector = worldPosition.xyz - lightPos;
}
