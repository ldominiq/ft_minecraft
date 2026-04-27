#version 460 core
layout (location = 0) in vec3 aPos;

out vec4 clipSpace;
out vec3 toCameraVector;
out vec2 textureCoords;
out vec3 fromLightVector;
out float lightPosYOut;

uniform mat4 projection;
uniform mat4 viewRot;
uniform vec3 chunkRel;
uniform vec3 chunkOriginWorld;
uniform vec3 lightPositionRel;
uniform float lightPosY;

uniform float tiling;

void main() {
    vec3 worldPos = chunkOriginWorld + aPos;
    vec3 cameraRelPos = chunkRel + aPos;

    // Clip space coordinates for projective texture mapping
    clipSpace = projection * viewRot * vec4(cameraRelPos, 1.0);
    gl_Position = clipSpace;

    textureCoords = worldPos.xz * tiling;
    
    // Calculate vectors for lighting and Fresnel
    toCameraVector = -cameraRelPos;

    fromLightVector = cameraRelPos - lightPositionRel;

    lightPosYOut = lightPosY;
}
