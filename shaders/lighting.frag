#version 460 core

in VS_OUT {
    vec3 FragPos;
    vec3 FragPosRel;
    vec3 Normal;
    vec2 TexCoord;
    float TexLayer;
    float SkyLight; // 0.0 = fully underground, 1.0 = open sky
    float WaterAbove; // 1.0 iff this face has a water block directly above
    float BlockLight; // Emissive torch light, 0..1 (not sky-modulated)
} fs_in;

out vec4 FragColor;

struct Material {
    sampler2D diffuse;
    sampler2D specular;    
    float shininess;
}; 

struct DirLight {
    vec3 direction;
	
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct PointLight {
    vec3 position;
    
    float constant;
    float linear;
    float quadratic;
	
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

struct SpotLight {
    vec3 position;
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

struct Shadows {
    bool enabled;
    float MIN_BIAS;
    float MAX_BIAS;
};

#define NR_POINT_LIGHTS 3
#define MAX_CASCADES 5
#define MAX_SPOT_LIGHTS 16

uniform bool debugCascades;   // toggle from ImGui
int debugCascadeLayer = -1;   // set by CSMShadowCalculation

uniform sampler2DArray blockTextures;
uniform DirLight dirLight;
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform SpotLight spotLights[MAX_SPOT_LIGHTS];
uniform int numSpotLights;
uniform Material material;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform int renderType;
uniform bool blinn;

uniform Shadows shadows;

// CSM uniforms
uniform sampler2DArrayShadow shadowMapArray;
uniform mat4 lightSpaceMatrices[MAX_CASCADES];
uniform float cascadePlaneDistances[MAX_CASCADES - 1]; // N-1 split points for N cascades
uniform int cascadeCount;
uniform float farPlane;
uniform mat4 viewRot;
uniform int pcfQuality; // 0=1-tap, 1=3x3, 2=5x5

// Toggles the texColor.a < 0.1 discard (leaf/glass cutouts).
// Set to false by Fast leaf-render mode for max early-Z efficiency.
uniform bool useAlphaTest;

// SSAO
uniform sampler2D ssaoTexture;
uniform int ssaoEnabled;
uniform vec2 screenSize; // Full viewport resolution for correct SSAO UV mapping

// Underwater params
uniform bool cameraUnderwater;
uniform vec3 underwaterTintColor;
uniform vec3 underwaterFogColor;
uniform float underwaterFogDensity;

// caustics
uniform float seaLevel;
uniform float causticTime;
uniform sampler2D causticsMap;
// Off in passes that render without shadow info (water refraction), where
// the caustic block can't gate by sun occlusion and would otherwise light
// shadowed underwater terrain that's visible through the water surface.
uniform float causticsEnabled;

// Distance fog (sky LUT blending)
uniform sampler2D skyLUT;
uniform float skyExposure;
uniform float fogStart;    // world-space distance where fog begins
uniform float fogEnd;      // world-space distance where fog is fully opaque
uniform float fogStrength; // exponent: 1=linear ramp, >1=fog concentrated at edge
uniform bool fogEnabled;
// HDR pipeline: keep fog linear (HDR) so it gets tonemapped once at the end.
uniform bool hdrMode;

#include "sky_common.glsl"

// Caustics from a texture (assets/textures/caustics.jpg). Two layers panning
// in different directions and at different scales
float computeCaustic(vec2 worldXZ, float t) {
    float ts = t * 0.04;
    vec2 uv1 = worldXZ * 0.06 + vec2( ts,  ts * 0.7);
    vec2 uv2 = worldXZ * 0.10 + vec2(-ts * 0.6, ts * 0.4);
    float c1 = texture(causticsMap, uv1).r;
    float c2 = texture(causticsMap, uv2).r;
    return max(c1, c2);
}

float near = 0.1;
float far  = 100.0;

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // back to NDC
    return (2.0 * near * far) / (far + near - z * (far - near));
}

// function prototypes
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, float ao, vec3 texCol);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, float ao, vec3 texCol);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, float ao, vec3 texCol);
float CSMShadowCalculation(vec3 fragPosRel);
float sampleCascadeShadow(int layer, vec3 fragPosRel, vec3 normal, vec3 lightDir);

void main()
{
    // Sample the texture array using (u, v, layer)
    vec4 texColor = texture(blockTextures, vec3(fs_in.TexCoord, fs_in.TexLayer));

    // Alpha test (cutout discard) - toggleable. In "Fast" leaf mode the host
    // sets useAlphaTest=false so leaf cubes render fully opaque (no cutouts)
    // and the GPU keeps early-Z fully effective.
    if (useAlphaTest && texColor.a < 0.1)
        discard;

    // Unpremultiply alpha to get original colors (only for semi-transparent pixels)
    // For opaque or nearly-opaque pixels (alpha > 0.95), skip to avoid precision issues
    if (useAlphaTest && texColor.a > 0.01 && texColor.a < 0.95) {
        texColor.rgb /= texColor.a;
    }

    // properties
    vec3 color = texColor.rgb;
    vec3 norm = normalize(fs_in.Normal);
    vec3 viewDir = normalize(-fs_in.FragPosRel);
    
    // SSAO
    float AmbientOcclusion = 1.0;
    if (ssaoEnabled == 1) {
        // Use full viewport size, not texture size - the SSAO texture may
        // be half-resolution (when blur is disabled + halfRes is on).
        vec2 ssaoUV = gl_FragCoord.xy / screenSize;
        AmbientOcclusion = texture(ssaoTexture, ssaoUV).r;
    }

    // == =====================================================
    // Our lighting is set up in 3 phases: directional, point lights and an optional flashlight
    // For each phase, a calculate function is defined that calculates the corresponding color
    // per lamp. In the main() function we take all the calculated colors and sum them up for
    // this fragment's final color.
    // == =====================================================
    // phase 1: directional lighting
    vec3 result = CalcDirLight(dirLight, norm, viewDir, AmbientOcclusion, color);
    // phase 2: point lights
    for(int i = 0; i < NR_POINT_LIGHTS; i++)
        result += CalcPointLight(pointLights[i], norm, fs_in.FragPosRel, viewDir, AmbientOcclusion, color);
    // phase 3: spot lights (local flashlight + any remote players whose
    // flashlights are on this frame - host caps at MAX_SPOT_LIGHTS).
    for (int i = 0; i < numSpotLights; ++i)
        result += CalcSpotLight(spotLights[i], norm, fs_in.FragPosRel, viewDir, AmbientOcclusion, color);

    // Caustics - only on faces that are *actually* under a water block.
    // The WaterAbove flag is baked at mesh time (top faces whose +Y
    // neighbour is water). This is stricter than "below sea level" (which
    // lit cave floors) and stricter than SkyLight (which leaks sideways
    // into caves through the chunk light map).
    if (causticsEnabled > 0.5 && fs_in.WaterAbove > 0.5 && dirLight.direction.y < 0.0) {
        float c = computeCaustic(fs_in.FragPos.xz, causticTime);
        float depthBelow = max(seaLevel - fs_in.FragPos.y, 0.0);
        float nearFade = smoothstep(0.0, 0.8, depthBelow);
        float farFade  = clamp(1.0 - depthBelow / 20.0, 0.0, 1.0);
        float depthFade = nearFade * farFade;
        // Only upward-facing surfaces receive caustics; cave ceilings get nothing.
        float upFactor   = max(norm.y, 0.0);
        // Sun-elevation gate, matching the day/night fade used by directional light.
        float sunUp      = clamp(-dirLight.direction.y, 0.0, 1.0);
        // AO modulates so caustics don't blast through occluded crevices.
        // prevent the caustics to show in shadowed areas
        float shadow = 0.0;
        if (shadows.enabled && fs_in.SkyLight > 0.01) {
            shadow = CSMShadowCalculation(fs_in.FragPosRel);
        }
        float shadowGate = 1.0 - shadow;
        float strength   = c * depthFade * upFactor * sunUp * shadowGate * 2.0 * AmbientOcclusion;
        result += dirLight.diffuse * strength;
    }

    // Emissive torch block-light. Baked per-vertex (BFS in the chunk), so
    // this is free per-fragment and scales to unlimited placed torches.
    result += vec3(1.0, 0.62, 0.30) * (fs_in.BlockLight * fs_in.BlockLight) * 1.6;

    if (renderType == 1) {
        FragColor = vec4(norm * 0.5 + 0.5, 1.0); // Visualize normals
    }
    else if (renderType == 2) {
        float depth = LinearizeDepth(gl_FragCoord.z) / far; // Visualize depth buffer
        FragColor = vec4(vec3(depth), 1.0);

    } else {
        vec3 finalColor = result * color;
        if (fogEnabled && !cameraUnderwater) {
            float dist = length(fs_in.FragPosRel);
            float fogFactor = 1.0 - pow(smoothstep(fogStart, fogEnd, dist), fogStrength);
            vec3 fogDir = normalize(fs_in.FragPosRel);
            vec3 fogColor = hdrMode
                ? sampleSkyColorLinear(skyLUT, fogDir, normalize(-dirLight.direction))
                : sampleSkyColor(skyLUT, fogDir, normalize(-dirLight.direction), skyExposure);
            finalColor = mix(fogColor, finalColor, fogFactor);
        }
        FragColor = vec4(finalColor, 1.0);
    }

    // ── Cascade debug overlay ──
    // Tints each cascade a different color so you can see the boundaries
    if (debugCascades && debugCascadeLayer >= 0)
    {
        vec3 cascadeColor;
        if (debugCascadeLayer == 0)
            cascadeColor = vec3(1.0, 0.0, 0.0);  // Red   - closest
        else if (debugCascadeLayer == 1)
            cascadeColor = vec3(0.0, 1.0, 0.0);  // Green
        else if (debugCascadeLayer == 2)
            cascadeColor = vec3(0.0, 0.0, 1.0);  // Blue
        else if (debugCascadeLayer == 3)
            cascadeColor = vec3(1.0, 1.0, 0.0);  // Yellow
        else
            cascadeColor = vec3(1.0, 0.0, 1.0);  // Magenta - farthest

        // Mix: 80% original color + 20% cascade tint
        FragColor = vec4(mix(FragColor.rgb, cascadeColor, 0.2), FragColor.a);
    }

    if (cameraUnderwater) {
        vec3 tintedColor = FragColor.rgb * underwaterTintColor;

        // Distance based fog
        float distance = length(fs_in.FragPosRel);
        float fogFactor = exp(-distance * underwaterFogDensity);
        fogFactor = clamp(fogFactor, 0.0, 1.0);

        // In HDR mode the final composite re-applies gamma; lift the sRGB-authored
        // fog color to linear so it survives the pow(1/2.2) without going pale.
        vec3 fogCol = hdrMode ? pow(underwaterFogColor, vec3(2.2)) : underwaterFogColor;
        FragColor.rgb = mix(fogCol, tintedColor, fogFactor);
    }

    //FragColor = vec4(lighting, texColor.a); // Lighting
    //FragColor = vec4(fs_in.TexCoord, 0.0, 1.0); // Visualize texture coordinates
}

float CSMShadowCalculation(vec3 fragPosRel)
{
    if (cascadeCount == 0)
        return 0.0;

    // 1. Find fragment depth in VIEW SPACE.
    //    We need to know how far this fragment is from the camera
    //    so we can pick the right cascade.
    vec4 fragPosViewSpace = viewRot * vec4(fragPosRel, 1.0);
    float depthValue = abs(fragPosViewSpace.z);

    // 2. Select the cascade layer.
    //    Walk through the split distances until we find the first
    //    cascade whose far plane is beyond our depth.
    int layer = cascadeCount - 1;
    for (int i = 0; i < cascadeCount - 1; ++i)
    {
        if (depthValue < cascadePlaneDistances[i])
        {
            layer = i;
            break;
        }
    }

    // Store for debug visualization
    debugCascadeLayer = layer;

    // 3. Out-of-bounds check on primary cascade
    vec4 fragPosLightSpace = lightSpaceMatrices[layer] * vec4(fragPosRel, 1.0);

    // Perspective divide (ortho makes w=1, but good practice)
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;

    // Transform from [-1,1] NDC to [0,1] texture coordinates
    projCoords = projCoords * 0.5 + 0.5;

    // 4. Out-of-bounds checks.
    //    If the fragment projects outside the shadow map in XY or beyond
    //    the far plane in Z, treat it as unshadowed (no data available).
    if (projCoords.z > 1.0 ||
        projCoords.x < 0.0 || projCoords.x > 1.0 ||
        projCoords.y < 0.0 || projCoords.y > 1.0)
        return 0.0;

    // 5. Bias - scale proportional to the texel size of this cascade.
    //    Larger cascades cover more world space per texel, so they
    //    need proportionally more bias.  We derive the scale from the
    //    shadow map resolution vs the cascade's projected extent (which
    //    is encoded implicitly in the texel size of the projCoords).
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightDir = normalize(-dirLight.direction);

    // 6. Sample primary cascade
    float shadow = sampleCascadeShadow(layer, fragPosRel, normal, lightDir);

    // 7. Blend between cascades near the boundary to hide the seam.
    //    In the last 20% of each cascade's range we linearly blend
    //    with the next cascade's shadow value.
    if (layer < cascadeCount - 1)
    {
        float cascadeFar = cascadePlaneDistances[layer];
        float blendStart = cascadeFar * 0.8;  // start blending at 80% of cascade range

        if (depthValue > blendStart)
        {
            float blendFactor = clamp((depthValue - blendStart) / (cascadeFar - blendStart), 0.0, 1.0);
            float nextShadow = sampleCascadeShadow(layer + 1, fragPosRel, normal, lightDir);
            shadow = mix(shadow, nextShadow, blendFactor);
        }
    }

    // 8. Fade shadow at the edge of the last cascade to avoid hard cutoff.
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

// Helper: compute shadow for a single cascade layer.
// Returns shadow in [0,1] where 1 = fully in shadow.
float sampleCascadeShadow(int layer, vec3 fragPosRel, vec3 normal, vec3 lightDir)
{
    float ndotl = max(dot(normal, lightDir), 0.0);
    float baseBias = max(shadows.MAX_BIAS * (1.0 - ndotl), shadows.MIN_BIAS);

    // Each successive cascade covers roughly 4× the area of the previous,
    // so texel size doubles.  Scale bias accordingly.
    float cascadeScale = 1.0 + float(layer) * 0.5;
    float bias = baseBias * cascadeScale;

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMapArray, 0));
    float normalOffsetScale = texelSize.x * cascadeScale * 3.0;
    vec3 offsetPos = fragPosRel + normal * normalOffsetScale * (1.0 - ndotl);

    vec4 fragPosLightSpaceOffset = lightSpaceMatrices[layer] * vec4(offsetPos, 1.0);
    vec3 offsetCoords = fragPosLightSpaceOffset.xyz / fragPosLightSpaceOffset.w;
    offsetCoords = offsetCoords * 0.5 + 0.5;

    // Out-of-bounds → no shadow
    if (offsetCoords.z > 1.0 ||
        offsetCoords.x < 0.0 || offsetCoords.x > 1.0 ||
        offsetCoords.y < 0.0 || offsetCoords.y > 1.0)
        return 0.0;

    float biasedDepth = offsetCoords.z - bias;

    // PCF: 3×3 for cascade 0, 5×5 for farther cascades
    float shadow = 0.0;
    if (pcfQuality == 0) {
        shadow = texture(shadowMapArray, vec4(offsetCoords.xy, float(layer), biasedDepth));
    } else if (pcfQuality == 1) {
        for (int x = -1; x <= 1; ++x)
            for (int y = -1; y <= 1; ++y)
            {
                vec2 sampleUV = offsetCoords.xy + vec2(x, y) * texelSize;
                shadow += texture(shadowMapArray, vec4(sampleUV, float(layer), biasedDepth));
            }
        shadow /= 9.0;
    } else {
        for (int x = -2; x <= 2; ++x)
            for (int y = -2; y <= 2; ++y)
            {
                vec2 sampleUV = offsetCoords.xy + vec2(x, y) * texelSize;
                shadow += texture(shadowMapArray, vec4(sampleUV, float(layer), biasedDepth));
            }
        shadow /= 25.0;
    }

    // Invert: hardware returns 1=lit, we want 1=shadow
    shadow = 1.0 - shadow;

    // Attenuate distant cascades
    if (layer >= 1)
    {
        float cascadeAttenuation = 1.0 - float(layer) * 0.2;
        shadow *= clamp(cascadeAttenuation, 0.3, 1.0);
    }

    return shadow;
}

// calculates the color when using a directional light.
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, float ao, vec3 texCol)
{
    vec3 lightDir = normalize(-light.direction);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    float spec = 0.0;
    if(blinn)
    {
        vec3 halfwayDir = normalize(lightDir + viewDir);  
        spec = pow(max(dot(normal, halfwayDir), 0.0), 16.0);
    }
    else
    {
        vec3 reflectDir = reflect(-lightDir, normal);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), 8.0);
    }
    // combine results
    vec3 ambient = light.ambient * texCol;
    vec3 diffuse = light.diffuse * diff * texCol;
    vec3 specular = light.specular * spec * texCol;

    // calculate shadow
    float shadow = 0.0;
    if (shadows.enabled)
        if (light.direction.y < 0.0 && fs_in.SkyLight > 0.01)
        {
            shadow = CSMShadowCalculation(fs_in.FragPosRel);
        }

    // ── Sky-light modulation ────────────────────────────────────
    // fs_in.SkyLight is 0.0 deep in caves, 1.0 under open sky.
    // It controls how much natural light (ambient AND direct sun)
    // reaches this block.
    //
    // MIN_CAVE_LIGHT keeps caves from being pitch black.
    const float MIN_CAVE_LIGHT = 0.04;
    float skyFactor = fs_in.SkyLight;

    // Ambient: in a cave (skyFactor ≈ 0) drops to MIN_CAVE_LIGHT.
    ambient *= max(skyFactor, MIN_CAVE_LIGHT);

    // Diffuse & specular: also scaled by skyFactor.
    // Without this, caves lit by the sun at an angle (no terrain
    // between sun and cave interior from the CSM's perspective)
    // would still receive full diffuse/specular - looking bright
    // underground.  skyFactor tells us the block is enclosed, so
    // direct sunlight shouldn't reach it regardless of the shadow
    // map's opinion.
    diffuse  *= skyFactor;
    specular *= skyFactor;

    // Also darken ambient in shadowed areas.  Without this, faces
    // behind hills/terrain at sunset still glow because diffuse is
    // near zero (low sun angle) and ambient bypasses the shadow map.
    // We blend: in shadow, ambient drops to 50% of its value (was 30%).
    // Raised so daytime shadows stay readable; exposure adaptation in HDR
    // mode compensates for any overall brightness shift.
    float ambientShadowFactor = 1.0 - shadow * 0.5;
    ambient *= ambientShadowFactor;

    // SSAO: darken ambient by screen-space occlusion
    ambient *= ao;

    vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular));    
    return (lighting);
}

// calculates the color when using a point light.
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, float ao, vec3 texCol)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    float spec = 0.0;
    if(blinn)
    {
        vec3 halfwayDir = normalize(lightDir + viewDir);  
        spec = pow(max(dot(normal, halfwayDir), 0.0), 16.0);
    }
    else
    {
        vec3 reflectDir = reflect(-lightDir, normal);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), 8.0);
    }
    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    // combine results
    vec3 ambient = light.ambient * texCol;
    vec3 diffuse = light.diffuse * diff * texCol;
    vec3 specular = light.specular * spec * texCol;
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    ambient *= ao;
    return (ambient + diffuse + specular);
}

// calculates the color when using a spot light.
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, float ao, vec3 texCol)
{
    vec3 lightDir = normalize(light.position - fragPos);
    // diffuse shading
    float diff = max(dot(normal, lightDir), 0.0);
    // specular shading
    float spec = 0.0;
    if(blinn)
    {
        vec3 halfwayDir = normalize(lightDir + viewDir);  
        spec = pow(max(dot(normal, halfwayDir), 0.0), 16.0);
    }
    else
    {
        vec3 reflectDir = reflect(-lightDir, normal);
        spec = pow(max(dot(viewDir, reflectDir), 0.0), 8.0);
    }
    // attenuation
    float distance = length(light.position - fragPos);
    float attenuation = 1.0 / (light.constant + light.linear * distance + light.quadratic * (distance * distance));    
    // spotlight intensity
    float theta = dot(lightDir, normalize(-light.direction)); 
    float epsilon = light.cutOff - light.outerCutOff;
    float intensity = clamp((theta - light.outerCutOff) / epsilon, 0.0, 1.0);
    // combine results
    vec3 ambient = light.ambient * texCol;
    vec3 diffuse = light.diffuse * diff * texCol;
    vec3 specular = light.specular * spec * texCol;
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    ambient *= ao;
    return (ambient + diffuse + specular);
}
