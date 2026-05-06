#version 460 core
layout (location = 0) in vec3 aPos;       // Vertex position (local to plant model)
layout (location = 1) in vec2 aTexCoord;  // Texture coordinates
layout (location = 2) in vec3 aNormal;    // Normal vector

// Per-instance attributes
layout (location = 3) in vec3 aInstancePos;    // Chunk-local position of vegetation instance
layout (location = 4) in float aTexLayer;      // Texture layer index for this instance
layout (location = 5) in float aRotation;      // Random rotation around Y-axis
layout (location = 6) in float aSkyLight;      // Sky-light level (0.0 = dark, 1.0 = full sun)
layout (location = 7) in float aColumnBaseY;   // Y of the bottom block in this column (for coherent sway)
layout (location = 8) in float aAOFactor;      // Ambient occlusion factor (1.0 = bright, 0.3 = dark)
layout (location = 9) in float aBlockLight;    // Block light level (0.0 = no light, 1.0 = full torch light)

out VS_OUT {
    vec3 FragPos;
    vec3 FragPosRel;
    vec3 Normal;
    vec2 TexCoord;
    float TexLayer;
    float SkyLight;
    float IsUnderwater;
    float AOFactor;
    float BlockLight;
} vs_out;

uniform mat4 projection;
uniform mat4 view;
uniform mat4 viewRot;
uniform vec3 chunkRel;
uniform vec3 chunkOriginWorld;
uniform vec4 clipPlane;
uniform float time;
uniform float seaLevel;

// Graphics-quality knobs (all set from Renderer settings):
//   vegetationSwayQuality  0 = no sway (cheapest), 1 = single sin, 2 = full
//   vegetationSwayMaxDist  fade sway to zero past this distance; 0 = no fade
//   vegetationDensity      render every Nth instance (1 = all)
uniform int   vegetationSwayQuality;
uniform float vegetationSwayMaxDist;
uniform int   vegetationDensity;

void main() {
    // ── Density culling: cheapest path wins ─────────────────────────
    // Park skipped instances at clip-space (2,2,2,1) so they're trivially
    // culled — vertex still runs but exits before the expensive math below.
    if (vegetationDensity > 1 && (gl_InstanceID % vegetationDensity) != 0) {
        gl_Position = vec4(2.0, 2.0, 2.0, 1.0);
        gl_ClipDistance[0] = -1.0;
        return;
    }

    // Apply rotation around Y-axis for variety
    float cosRot = cos(aRotation);
    float sinRot = sin(aRotation);
    mat3 rotationMatrix = mat3(
        cosRot, 0.0, sinRot,
        0.0,    1.0, 0.0,
       -sinRot, 0.0, cosRot
    );

    vec3 rotatedPos = rotationMatrix * aPos;
    vec3 localPos = rotatedPos + aInstancePos;
    vec3 worldPos = chunkOriginWorld + localPos;

    // Determine if this vegetation is underwater
    bool isUnderwater = (worldPos.y < seaLevel);

    // ── Sway: skip entirely on quality 0 ────────────────────────────
    // Uniform branch — coherent across all fragments, so the GPU only
    // executes the path that's selected; the others cost nothing.
    if (vegetationSwayQuality > 0) {
        // Distance-based LOD: fade sway out as we approach the cutoff.
        // (Cheap dot-product distance, no sqrt needed for the comparison.)
        float swayFade = 1.0;
        if (vegetationSwayMaxDist > 0.0) {
            vec3 camRel = localPos + chunkRel;
            float distSq = dot(camRel, camRel);
            float fadeStart = vegetationSwayMaxDist * 0.7;
            float fadeStartSq = fadeStart * fadeStart;
            float fadeEndSq   = vegetationSwayMaxDist * vegetationSwayMaxDist;
            swayFade = 1.0 - clamp((distSq - fadeStartSq) / max(fadeEndSq - fadeStartSq, 1e-3),
                                   0.0, 1.0);
        }

        if (swayFade > 0.001) {
            float plantPhase = aRotation * (isUnderwater ? 2.17 : 1.73);
            float swayX = 0.0;
            float swayZ = 0.0;

            if (isUnderwater) {
                float h = worldPos.y - aColumnBaseY;
                float bend = h * h * 0.012;

                if (vegetationSwayQuality == 1) {
                    // Cheap underwater: one sin, elliptical-ish via phase offset
                    float s = sin(time * 0.35 + worldPos.x * 0.4 + plantPhase);
                    swayX = bend * s;
                    swayZ = bend * 0.7 * s; // reuse the sin to avoid a second call
                } else {
                    // Full quality — original 5-sin organic motion
                    swayX = bend * sin(time * 0.35 + worldPos.x * 0.4 + worldPos.z * 0.25 + plantPhase);
                    swayZ = bend * sin(time * 0.28 + worldPos.x * 0.3 + worldPos.z * 0.5 + plantPhase + 1.57);
                    swayX += bend * 0.3  * sin(time * 0.6 + worldPos.z * 0.7 + plantPhase * 0.5);
                    swayZ += bend * 0.25 * sin(time * 0.5 + worldPos.x * 0.6 + plantPhase * 0.7);
                    float ripple = h * 0.008 * sin(time * 1.8 - h * 2.0 + plantPhase);
                    swayX += ripple;
                    swayZ -= ripple * 0.7;
                }
            } else {
                if (vegetationSwayQuality == 1) {
                    // Cheap land: single sin, X-only sway
                    float s = sin(time * 1.5 + worldPos.x * 0.8 + worldPos.z * 0.6 + plantPhase);
                    swayX = aPos.y * 0.08 * s;
                    swayZ = swayX * 0.3; // reuse the sin
                } else {
                    // Full quality — original 3-sin land sway
                    float sway = aPos.y * 0.08
                        * sin(time * 1.5 + worldPos.x * 0.8 + worldPos.z * 0.6 + plantPhase)
                        + aPos.y * 0.03
                        * sin(time * 2.3 + worldPos.x * 1.4 + worldPos.z * 1.1 + plantPhase * 0.6);
                    swayX = sway;
                    swayZ = sway * 0.5 * sin(time * 1.1 + plantPhase);
                }
            }

            swayX *= swayFade;
            swayZ *= swayFade;
            worldPos.x += swayX;
            worldPos.z += swayZ;
            localPos.x += swayX;
            localPos.z += swayZ;
        }
    }

    vec3 cameraRelPos = localPos + chunkRel;

    vs_out.FragPos = worldPos;
    vs_out.FragPosRel = cameraRelPos;
    vs_out.Normal = rotationMatrix * aNormal;
    vs_out.TexCoord = aTexCoord;
    vs_out.TexLayer = aTexLayer;
    vs_out.SkyLight = aSkyLight;
    vs_out.IsUnderwater = isUnderwater ? 1.0 : 0.0;
    vs_out.AOFactor = aAOFactor;
    vs_out.BlockLight = aBlockLight;

    gl_Position = projection * viewRot * vec4(cameraRelPos, 1.0);
    gl_ClipDistance[0] = dot(vec4(worldPos, 1.0), clipPlane);
}
