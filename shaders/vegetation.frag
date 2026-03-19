#version 460 core

in VS_OUT {
    vec3 FragPos;
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

// CSM shadow uniforms
#define MAX_CASCADES 5
uniform sampler2DArrayShadow shadowMapArray;
uniform mat4 lightSpaceMatrices[MAX_CASCADES];
uniform float cascadePlaneDistances[MAX_CASCADES - 1];
uniform int cascadeCount;
uniform float farPlane;
uniform mat4 view;
uniform bool shadowsEnabled;

// Forward declarations
float computeVegetationShadow(vec3 fragPosWorldSpace);

void main() {
    // Sample texture from array (premultiplied alpha)
    vec4 texColor = texture(blockTextures, vec3(fs_in.TexCoord, fs_in.TexLayer));

    // Discard transparent pixels — threshold raised to catch semi-transparent
    // mipmap edge pixels that would otherwise occlude terrain behind
    if (texColor.a < 0.5)
        discard;

    // Unpremultiply alpha to get original colors (only for semi-transparent pixels)
    // For opaque or nearly-opaque pixels (alpha > 0.95), skip to avoid precision issues
    if (texColor.a > 0.01 && texColor.a < 0.95) {
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
        shadow = computeVegetationShadow(fs_in.FragPos);
    }

    // Combine skylight and block light: use the maximum of the two
    // Block light is attenuated (0.6x) to avoid overpowering natural light
    float effectiveLight = max(fs_in.SkyLight, fs_in.BlockLight * 0.6);

    // Ambient + diffuse, modulated by AO factor and CSM shadow
    // AO affects both ambient and diffuse for enclosed space darkening
    // Shadow only affects diffuse component (shadows don't block ambient/skylight fully)
    vec3 ambient = ambientColor * effectiveLight * fs_in.AOFactor;
    vec3 diffuse = lightColor * diff * effectiveLight * fs_in.AOFactor * (1.0 - shadow * 0.8);

    // Clamp to avoid overbright whites
    vec3 result = min(ambient + diffuse, vec3(1.0)) * texColor.rgb;

    // Apply underwater tint — blue-green color absorption
    if (fs_in.IsUnderwater > 0.5) {
        vec3 waterTint = vec3(0.4, 0.7, 0.6);
        result *= waterTint;
    }

    // Apply camera underwater fog (when viewing from underwater)
    if (cameraUnderwater) {
        // Distance-based fog for vegetation
        float distance = length(viewPos - fs_in.FragPos);
        float fogFactor = exp(-distance * underwaterFogDensity);
        fogFactor = clamp(fogFactor, 0.0, 1.0);

        vec3 tintedColor = result * underwaterTintColor;

        result = mix(underwaterFogColor, tintedColor, fogFactor);
    }

    FragColor = vec4(result, texColor.a);
}

// Simplified CSM shadow calculation for vegetation
// Uses simpler bias and no complex normal offsetting since vegetation uses upward normal
float computeVegetationShadow(vec3 fragPosWorldSpace)
{
    if (cascadeCount == 0)
        return 0.0;

    // Find fragment depth in view space to select cascade
    vec4 fragPosViewSpace = view * vec4(fragPosWorldSpace, 1.0);
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
    vec4 fragPosLightSpace = lightSpaceMatrices[layer] * vec4(fragPosWorldSpace, 1.0);
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

    // PCF sampling: 3x3 for first cascade, 5x5 for others
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMapArray, 0));
    float shadow = 0.0;

    if (layer == 0)
    {
        for (int x = -1; x <= 1; ++x)
            for (int y = -1; y <= 1; ++y)
            {
                vec2 sampleUV = projCoords.xy + vec2(x, y) * texelSize;
                shadow += texture(shadowMapArray, vec4(sampleUV, float(layer), biasedDepth));
            }
        shadow /= 9.0;
    }
    else
    {
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
            vec4 nextLightSpace = lightSpaceMatrices[layer + 1] * vec4(fragPosWorldSpace, 1.0);
            vec3 nextCoords = nextLightSpace.xyz / nextLightSpace.w;
            nextCoords = nextCoords * 0.5 + 0.5;

            if (!(nextCoords.z > 1.0 || nextCoords.x < 0.0 || nextCoords.x > 1.0 ||
                  nextCoords.y < 0.0 || nextCoords.y > 1.0))
            {
                float nextBias = baseBias * (1.0 + float(layer + 1) * 0.5) * (1.0 - ndotl * 0.5);
                float nextDepth = nextCoords.z - nextBias;
                float nextShadow = 0.0;

                for (int x = -2; x <= 2; ++x)
                    for (int y = -2; y <= 2; ++y)
                    {
                        vec2 sampleUV = nextCoords.xy + vec2(x, y) * texelSize;
                        nextShadow += texture(shadowMapArray, vec4(sampleUV, float(layer + 1), nextDepth));
                    }
                nextShadow = 1.0 - (nextShadow / 25.0);

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
