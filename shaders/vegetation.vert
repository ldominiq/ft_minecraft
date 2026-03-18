#version 460 core
layout (location = 0) in vec3 aPos;       // Vertex position (local to plant model)
layout (location = 1) in vec2 aTexCoord;  // Texture coordinates
layout (location = 2) in vec3 aNormal;    // Normal vector

// Per-instance attributes
layout (location = 3) in vec3 aInstancePos;    // World position of vegetation instance
layout (location = 4) in float aTexLayer;      // Texture layer index for this instance
layout (location = 5) in float aRotation;      // Random rotation around Y-axis
layout (location = 6) in float aSkyLight;      // Sky-light level (0.0 = dark, 1.0 = full sun)
layout (location = 7) in float aColumnBaseY;   // Y of the bottom block in this column (for coherent sway)

out VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    float TexLayer;
    float SkyLight;
    float IsUnderwater;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform vec4 clipPlane;
uniform float time;
uniform float seaLevel;

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

    // Determine if this vegetation is underwater
    bool isUnderwater = (aInstancePos.y < seaLevel);

    if (isUnderwater) {
        // Organic underwater sway — coherent across stacked blocks.
        float h = (aInstancePos.y + aPos.y) - aColumnBaseY;

        // Quadratic falloff: base is anchored, tip sways most (like a real stalk)
        float bend = h * h * 0.012;

        // Per-plant phase offset so neighbours don't move in lockstep
        float plantPhase = aRotation * 2.17;

        // Primary slow current — elliptical motion (X and Z have offset phases)
        float swayX = bend * sin(time * 0.35 + aInstancePos.x * 0.4 + aInstancePos.z * 0.25 + plantPhase);
        float swayZ = bend * sin(time * 0.28 + aInstancePos.x * 0.3 + aInstancePos.z * 0.5 + plantPhase + 1.57);

        // Secondary gentle drift at a different frequency
        swayX += bend * 0.3 * sin(time * 0.6 + aInstancePos.z * 0.7 + plantPhase * 0.5);
        swayZ += bend * 0.25 * sin(time * 0.5 + aInstancePos.x * 0.6 + plantPhase * 0.7);

        // Subtle ripple that travels up the stalk (small, high-freq wavelet)
        float ripple = h * 0.008 * sin(time * 1.8 - h * 2.0 + plantPhase);
        swayX += ripple;
        swayZ -= ripple * 0.7;

        worldPosition.x += swayX;
        worldPosition.z += swayZ;
    } else {
        // Wind sway for land vegetation
        // Per-plant phase offset
        float plantPhase = aRotation * 1.73;
        float sway = aPos.y * 0.08
            * sin(time * 1.5 + aInstancePos.x * 0.8 + aInstancePos.z * 0.6 + plantPhase)
            + aPos.y * 0.03
            * sin(time * 2.3 + aInstancePos.x * 1.4 + aInstancePos.z * 1.1 + plantPhase * 0.6);

        worldPosition.x += sway;
        worldPosition.z += sway * 0.5 * sin(time * 1.1 + plantPhase); // slight figure-8 in Z
        //TODO: remove lol
//        worldPosition.y += sway * 10;
    }

    vs_out.FragPos = worldPosition.xyz;
    vs_out.Normal = rotationMatrix * aNormal;
    vs_out.TexCoord = aTexCoord;
    vs_out.TexLayer = aTexLayer;
    vs_out.SkyLight = aSkyLight;
    vs_out.IsUnderwater = isUnderwater ? 1.0 : 0.0;

    gl_Position = projection * view * worldPosition;
    gl_ClipDistance[0] = dot(worldPosition, clipPlane);
}
