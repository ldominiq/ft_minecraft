#version 460 core
layout (location = 0) in vec3 aPos;       // Vertex position (local to plant model)
layout (location = 1) in vec2 aTexCoord;  // Texture coordinates
layout (location = 2) in vec3 aNormal;    // Normal vector

// Per-instance attributes
layout (location = 3) in vec3 aInstancePos;    // World position of vegetation instance
layout (location = 4) in float aTexLayer;      // Texture layer index for this instance
layout (location = 5) in float aRotation;      // Random rotation around Y-axis
layout (location = 6) in float aSkyLight;      // Sky-light level (0.0 = dark, 1.0 = full sun)

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    float TexLayer;
    float SkyLight;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform vec4 clipPlane;
uniform float time;

void main() {
    // Apply rotation around Y-axis for variety
    float cosRot = cos(aRotation);
    float sinRot = sin(aRotation);
    mat3 rotationMatrix = mat3(
        cosRot, 0.0, sinRot,
        0.0,    1.0, 0.0,
       -sinRot, 0.0, cosRot
    );

    vec3 rotatedPos = rotationMatrix * aPos;
    vec4 worldPosition = vec4(rotatedPos + aInstancePos, 1.0);

    // Wind sway — only affects upper vertices (aPos.y > 0)
    float amplitudeSin1 = 0.08;
    float speedSin1 = 1.5;

    float amplitudeSin2 = 0.03;
    float speedSin2 = 2.3;

    float sway = aPos.y * amplitudeSin1
        * sin(time * speedSin1 + aInstancePos.x * 0.8 + aInstancePos.z * 0.6)
        + aPos.y * amplitudeSin2
        * sin(time * speedSin2 + aInstancePos.x * 1.4 + aInstancePos.z * 1.1);
    worldPosition.x += sway;
    worldPosition.z += sway * 0.6;
    //TODO: remove lol - could be used to make big seaweed ???
    worldPosition.y += sway * 10;

    vs_out.FragPos = worldPosition.xyz;
    vs_out.Normal = rotationMatrix * aNormal;
    vs_out.TexCoord = aTexCoord;
    vs_out.TexLayer = aTexLayer;
    vs_out.SkyLight = aSkyLight;

    gl_Position = projection * view * worldPosition;
    gl_ClipDistance[0] = dot(worldPosition, clipPlane);
}
