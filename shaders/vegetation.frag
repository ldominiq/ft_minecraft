#version 460 core

in VS_OUT {
    vec3 FragPos;
    vec3 FragPosRel;
    vec3 Normal;
    vec2 TexCoord;
    float TexLayer;
    float SkyLight;
    float IsUnderwater;
    float AOFactor;
    float BlockLight;
} fs_in;

out vec4 FragColor;

uniform sampler2DArray blockTextures;
uniform vec3 viewPos;
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;

// Underwater fog (camera-level, not per-vegetation instance)
uniform bool cameraUnderwater;
uniform vec3 underwaterFogColor;
uniform float underwaterFogDensity;
uniform vec3 underwaterTintColor;

// Distance fog (sky LUT blending)
uniform sampler2D skyLUT;
uniform float skyExposure;
uniform float fogStart;
uniform float fogEnd;
uniform float fogStrength;
uniform bool fogEnabled;
uniform bool hdrMode; // HDR: keep fog linear, tonemap once at the end.

// CSM shadow uniforms
#define MAX_CASCADES 5
uniform sampler2DArrayShadow shadowMapArray;
uniform mat4 lightSpaceMatrices[MAX_CASCADES];
uniform float cascadePlaneDistances[MAX_CASCADES - 1];
uniform int cascadeCount;
uniform float farPlane;
uniform mat4 viewRot;
uniform bool shadowsEnabled;
uniform int pcfQuality; // 0=1-tap, 1=3x3, 2=5x5

#include "sky_common.glsl"

// Dynamic spot/point lights (held torches + flashlights). Field names match
// Lighting::uploadSpotLights so App::uploadActiveSpotLights lights this shader
// with no extra host code. A held torch is an omni light (cutOff=-1,
// outerCutOff=-2 → intensity 1 in every direction).
#define VEG_MAX_SPOT_LIGHTS 16
struct VegSpotLight {
    vec3 position;   // camera-relative
    vec3 direction;
    float cutOff;
    float outerCutOff;
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
uniform VegSpotLight spotLights[VEG_MAX_SPOT_LIGHTS];
uniform int numSpotLights;

// Forward declarations
float computeVegetationShadow(vec3 fragPosWorldSpace);

void main() {
    // Sample texture from array (premultiplied alpha)
    vec4 texColor = texture(blockTextures, vec3(fs_in.TexCoord, fs_in.TexLayer));

    // Discard transparent pixels — threshold raised to catch semi-transparent
    // mipmap edge pixels that would otherwise occlude terrain behind
    if (texColor.a < 0.5)
        discard;

    // Unpremultiply alpha to get original colors
    // Skip nearly-opaque pixels (alpha >= 0.95) to avoid precision issues
    if (texColor.a < 0.95) {
        texColor.rgb /= texColor.a;
    }

    // Use a fixed upward normal for vegetation to avoid angle-dependent
    // brightness from cross-pattern quad normals
    vec3 normal = vec3(0.0, 1.0, 0.0);
    vec3 lightDirNorm = normalize(-lightDir);
    float diff = max(dot(normal, lightDirNorm), 0.0);

    // Compute shadow from CSM (if enabled and in sunlight)
    float shadow = 0.0;
    if (shadowsEnabled && fs_in.SkyLight > 0.01 && lightDirNorm.y > 0.0) {
        shadow = computeVegetationShadow(fs_in.FragPosRel);
    }

    // Sun/sky term uses skylight only; baked torch light is added separately
    // below as a warm, sky/shadow-independent emissive term (a pure multiply
    // can't brighten a pitch-black cave where ambient/diffuse are ~0).
    float effectiveLight = fs_in.SkyLight;

    // Ambient + diffuse, modulated by AO factor and CSM shadow
    // AO affects both ambient and diffuse for enclosed space darkening
    // Shadow only affects diffuse component (shadows don't block ambient/skylight fully)
    vec3 ambient = ambientColor * effectiveLight * fs_in.AOFactor;
    vec3 diffuse = lightColor * diff * effectiveLight * fs_in.AOFactor * (1.0 - shadow * 0.8);

    // Clamp to avoid overbright whites
    vec3 result = min(ambient + diffuse, vec3(1.0)) * texColor.rgb;

    // Baked torch block-light: warm additive term (matches lighting.frag /
    // entity_lighting.glsl) so grass near a placed torch glows in the dark.
    result += vec3(1.0, 0.62, 0.30) * (fs_in.BlockLight * fs_in.BlockLight)
              * 1.6 * texColor.rgb;

    // Dynamic held-torch / flashlight contribution (additive). Vegetation
    // uses a flat upward normal, so soften the N·L so side-lit grass isn't
    // pitch black.
    for (int i = 0; i < numSpotLights; ++i) {
        vec3 L = spotLights[i].position - fs_in.FragPosRel;
        float d = length(L);
        vec3 Ln = d > 1e-4 ? L / d : vec3(0.0, 1.0, 0.0);
        float att = 1.0 / (spotLights[i].constant
                         + spotLights[i].linear * d
                         + spotLights[i].quadratic * d * d);
        float theta = dot(Ln, normalize(-spotLights[i].direction));
        float eps = spotLights[i].cutOff - spotLights[i].outerCutOff;
        float intensity = clamp((theta - spotLights[i].outerCutOff)
                                / max(eps, 1e-4), 0.0, 1.0);
        float ndotl = max(dot(normal, Ln), 0.25);
        result += spotLights[i].diffuse * ndotl * att * intensity * texColor.rgb;
    }

    // Apply underwater tint — blue-green color absorption
    if (fs_in.IsUnderwater > 0.5) {
        vec3 waterTint = vec3(0.4, 0.7, 0.6);
        result *= waterTint;
    }

    // Apply camera underwater fog (when viewing from underwater)
    if (cameraUnderwater) {
        // Distance-based fog for vegetation. Use FragPosRel so distance stays
        // accurate at large world coordinates (FragPos at 5M is float-quantized).
        float distance = length(fs_in.FragPosRel);
        float fogFactor = exp(-distance * underwaterFogDensity);
        fogFactor = clamp(fogFactor, 0.0, 1.0);

        vec3 tintedColor = result * underwaterTintColor;

        // Lift sRGB-authored fog color to linear in HDR (final gamma is applied later).
        vec3 fogCol = hdrMode ? pow(underwaterFogColor, vec3(2.2)) : underwaterFogColor;
        result = mix(fogCol, tintedColor, fogFactor);
    }


    if (fogEnabled && !cameraUnderwater) {
        float dist = length(fs_in.FragPosRel);
        float fogFactor = 1.0 - pow(smoothstep(fogStart, fogEnd, dist), fogStrength);
        vec3 fogDir = normalize(fs_in.FragPosRel);
        vec3 fogColor = hdrMode
            ? sampleSkyColorLinear(skyLUT, fogDir, -lightDir)
            : sampleSkyColor(skyLUT, fogDir, -lightDir, skyExposure);
        result = mix(fogColor, result, fogFactor);
    }

    FragColor = vec4(result, texColor.a);
}

// Simplified CSM shadow calculation for vegetation
// Uses simpler bias and no complex normal offsetting since vegetation uses upward normal
float computeVegetationShadow(vec3 fragPosRel)
{
    if (cascadeCount == 0)
        return 0.0;

    // Find fragment depth in view space to select cascade
    vec4 fragPosViewSpace = viewRot * vec4(fragPosRel, 1.0);
    float depthValue = abs(fragPosViewSpace.z);

    // Select cascade layer
    int layer = cascadeCount - 1;
    for (int i = 0; i < cascadeCount - 1; ++i)
    {
        if (depthValue < cascadePlaneDistances[i])
        {
            layer = i;
            break;
        }
    }

    // Transform to light space
    vec4 fragPosLightSpace = lightSpaceMatrices[layer] * vec4(fragPosRel, 1.0);
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    projCoords = projCoords * 0.5 + 0.5;

    // Out-of-bounds check
    if (projCoords.z > 1.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    // Simple bias for vegetation (upward normal = 0,1,0)
    vec3 lightDirNorm = normalize(-lightDir);
    float ndotl = max(lightDirNorm.y, 0.0); // dot with upward normal
    float baseBias = 0.002;
    float cascadeScale = 1.0 + float(layer) * 0.5;
    float bias = baseBias * cascadeScale * (1.0 - ndotl * 0.5);

    float biasedDepth = projCoords.z - bias;

    // PCF sampling: quality controlled by pcfQuality uniform (uniform branch = free)
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMapArray, 0));
    float shadow = 0.0;

    if (pcfQuality == 0) {
        shadow = texture(shadowMapArray, vec4(projCoords.xy, float(layer), biasedDepth));
    } else if (pcfQuality == 1) {
        for (int x = -1; x <= 1; ++x)
            for (int y = -1; y <= 1; ++y)
            {
                vec2 sampleUV = projCoords.xy + vec2(x, y) * texelSize;
                shadow += texture(shadowMapArray, vec4(sampleUV, float(layer), biasedDepth));
            }
        shadow /= 9.0;
    } else {
        for (int x = -2; x <= 2; ++x)
            for (int y = -2; y <= 2; ++y)
            {
                vec2 sampleUV = projCoords.xy + vec2(x, y) * texelSize;
                shadow += texture(shadowMapArray, vec4(sampleUV, float(layer), biasedDepth));
            }
        shadow /= 25.0;
    }

    // Invert: hardware returns 1=lit, we want 1=shadow
    shadow = 1.0 - shadow;

    // Blend with next cascade near boundary
    if (layer < cascadeCount - 1)
    {
        float cascadeFar = cascadePlaneDistances[layer];
        float blendStart = cascadeFar * 0.8;

        if (depthValue > blendStart)
        {
            float blendFactor = clamp((depthValue - blendStart) / (cascadeFar - blendStart), 0.0, 1.0);

            // Sample next cascade
            vec4 nextLightSpace = lightSpaceMatrices[layer + 1] * vec4(fragPosRel, 1.0);
            vec3 nextCoords = nextLightSpace.xyz / nextLightSpace.w;
            nextCoords = nextCoords * 0.5 + 0.5;

            if (!(nextCoords.z > 1.0 || nextCoords.x < 0.0 || nextCoords.x > 1.0 ||
                  nextCoords.y < 0.0 || nextCoords.y > 1.0))
            {
                float nextBias = baseBias * (1.0 + float(layer + 1) * 0.5) * (1.0 - ndotl * 0.5);
                float nextDepth = nextCoords.z - nextBias;
                float nextShadow = 0.0;

                if (pcfQuality == 0) {
                    nextShadow = texture(shadowMapArray, vec4(nextCoords.xy, float(layer + 1), nextDepth));
                } else if (pcfQuality == 1) {
                    for (int x = -1; x <= 1; ++x)
                        for (int y = -1; y <= 1; ++y)
                        {
                            vec2 sampleUV = nextCoords.xy + vec2(x, y) * texelSize;
                            nextShadow += texture(shadowMapArray, vec4(sampleUV, float(layer + 1), nextDepth));
                        }
                    nextShadow /= 9.0;
                } else {
                    for (int x = -2; x <= 2; ++x)
                        for (int y = -2; y <= 2; ++y)
                        {
                            vec2 sampleUV = nextCoords.xy + vec2(x, y) * texelSize;
                            nextShadow += texture(shadowMapArray, vec4(sampleUV, float(layer + 1), nextDepth));
                        }
                    nextShadow /= 25.0;
                }
                nextShadow = 1.0 - nextShadow;

                shadow = mix(shadow, nextShadow, blendFactor);
            }
        }
    }

    // Fade shadow at edge of last cascade
    if (layer == cascadeCount - 1)
    {
        float fadeStart = farPlane * 0.9;
        if (depthValue > fadeStart)
        {
            float t = (depthValue - fadeStart) / (farPlane - fadeStart);
            shadow *= 1.0 - clamp(t, 0.0, 1.0);
        }
    }

    return shadow;
}
