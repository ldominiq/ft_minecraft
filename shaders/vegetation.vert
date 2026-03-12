#version 460 core
layout (location = 0) in vec3 aPos;       // Vertex position (local to plant model)
layout (location = 1) in vec2 aTexCoord;  // Texture coordinates
layout (location = 2) in vec3 aNormal;    // Normal vector

// Per-instance attributes
layout (location = 3) in vec3 aInstancePos;    // World position of vegetation instance
layout (location = 4) in float aTexLayer;      // Texture layer index for this instance
layout (location = 5) in float aRotation;      // Random rotation around Y-axis

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

    vs_out.FragPos = worldPosition.xyz;
    vs_out.Normal = rotationMatrix * aNormal;
    vs_out.TexCoord = aTexCoord;
    vs_out.TexLayer = aTexLayer;
    vs_out.SkyLight = 1.0; // Vegetation is always at surface, fully lit

    gl_Position = projection * view * worldPosition;
    gl_ClipDistance[0] = dot(worldPosition, clipPlane);
}
