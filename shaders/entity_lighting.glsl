// Shared lighting + CSM helpers for entity-type shaders (mobs, players,
// dropped items). The uniform names here intentionally match the ones in
// lighting.frag so the existing Lighting::uploadLightingUniforms and
// uploadCSMUniforms calls light up these shaders without any new host code.
//
// Differences from the terrain pipeline:
//   - No specular (entities have no spec maps).
//   - No SSAO (entities are too small for screen-space AO to add anything).
//   - 3-tap PCF instead of up to 5x5 - entity shadow edges don't need it.

#define ENTITY_NR_POINT_LIGHTS 3
#define ENTITY_MAX_CASCADES 5
// Multi-player flashlight slots; must match lighting.frag's MAX_SPOT_LIGHTS.
#define ENTITY_MAX_SPOT_LIGHTS 16

struct EL_DirLight {
    vec3 direction;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
struct EL_PointLight {
    vec3 position;     // camera-relative (eyePos subtracted host-side)
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};
struct EL_Shadows {
    bool enabled;
    float MIN_BIAS;
    float MAX_BIAS;
};
struct EL_SpotLight {
    vec3 position;     // camera-relative (eyePos subtracted host-side)
    vec3 direction;
    float cutOff;       // cos(inner)
    float outerCutOff;  // cos(outer)
    float constant;
    float linear;
    float quadratic;
    vec3 ambient;
    vec3 diffuse;
    vec3 specular;
};

uniform EL_DirLight dirLight;
uniform EL_PointLight pointLights[ENTITY_NR_POINT_LIGHTS];
uniform EL_Shadows shadows;
uniform EL_SpotLight spotLights[ENTITY_MAX_SPOT_LIGHTS];
uniform int numSpotLights;

// CSM (same layout terrain uses; uploaded by Lighting::uploadCSMUniforms).
uniform sampler2DArrayShadow shadowMapArray;
uniform mat4 lightSpaceMatrices[ENTITY_MAX_CASCADES];
uniform float cascadePlaneDistances[ENTITY_MAX_CASCADES - 1];
uniform int cascadeCount;
uniform float farPlane;
uniform mat4 viewRot;

// Returns shadow factor in [0,1]; 0 = fully lit, 1 = fully shadowed.
float entityCSMShadow(vec3 fragPosRel, vec3 normal, vec3 lightDir) {
    if (cascadeCount == 0 || !shadows.enabled) return 0.0;

    // Cascade selection by view-space depth.
    vec4 fpv = viewRot * vec4(fragPosRel, 1.0);
    float depth = abs(fpv.z);
    int layer = cascadeCount - 1;
    for (int i = 0; i < cascadeCount - 1; ++i) {
        if (depth < cascadePlaneDistances[i]) { layer = i; break; }
    }

    // Bias scales with cascade size (each cascade ~4x previous area → ~2x texel).
    float ndotl = max(dot(normal, lightDir), 0.0);
    float cascadeScale = 1.0 + float(layer) * 0.5;
    float bias = max(shadows.MAX_BIAS * (1.0 - ndotl), shadows.MIN_BIAS) * cascadeScale;

    vec2 texelSize = 1.0 / vec2(textureSize(shadowMapArray, 0));
    float normalOffsetScale = texelSize.x * cascadeScale * 3.0;
    vec3 offsetPos = fragPosRel + normal * normalOffsetScale * (1.0 - ndotl);

    vec4 lp = lightSpaceMatrices[layer] * vec4(offsetPos, 1.0);
    vec3 pc = lp.xyz / lp.w * 0.5 + 0.5;
    if (pc.z > 1.0 || pc.x < 0.0 || pc.x > 1.0 || pc.y < 0.0 || pc.y > 1.0)
        return 0.0;

    float biasedZ = pc.z - bias;
    // 3-tap PCF - cheap, sufficient for entity-scale geometry.
    float lit = 0.0;
    lit += texture(shadowMapArray, vec4(pc.xy,                  layer, biasedZ));
    lit += texture(shadowMapArray, vec4(pc.xy + texelSize,       layer, biasedZ));
    lit += texture(shadowMapArray, vec4(pc.xy - texelSize,       layer, biasedZ));
    return 1.0 - lit / 3.0;
}

// Lambertian point-light accumulation (no specular).
vec3 entityPointLightsContrib(vec3 fragPosRel, vec3 normal) {
    vec3 sum = vec3(0.0);
    for (int i = 0; i < ENTITY_NR_POINT_LIGHTS; ++i) {
        vec3 toLight = pointLights[i].position - fragPosRel;
        float dist = length(toLight);
        vec3 lightDir = toLight / max(dist, 1e-4);
        float ndotl = max(dot(normal, lightDir), 0.0);
        float att = 1.0 / (pointLights[i].constant
                         + pointLights[i].linear    * dist
                         + pointLights[i].quadratic * dist * dist);
        sum += (pointLights[i].diffuse * ndotl + pointLights[i].ambient * 0.3) * att;
    }
    return sum;
}

// Spot light (single slot) - Lambertian with attenuation + cone falloff.
// Mirrors lighting.frag::CalcSpotLight minus specular.
vec3 entitySpotLightContribOne(EL_SpotLight s, vec3 fragPosRel, vec3 normal) {
    vec3 toLight = s.position - fragPosRel;
    float dist = length(toLight);
    vec3 lightDir = toLight / max(dist, 1e-4);
    float ndotl = max(dot(normal, lightDir), 0.0);

    float att = 1.0 / (s.constant + s.linear * dist + s.quadratic * dist * dist);

    // Cone falloff: cosCutOff values are pre-cosined host-side.
    float theta   = dot(lightDir, normalize(-s.direction));
    float epsilon = s.cutOff - s.outerCutOff;
    float intensity = clamp((theta - s.outerCutOff) / max(epsilon, 1e-4), 0.0, 1.0);

    return (s.diffuse * ndotl + s.ambient * 0.3) * att * intensity;
}

// Sum contributions from every active spot light (local + remote players).
vec3 entitySpotLightContrib(vec3 fragPosRel, vec3 normal) {
    vec3 sum = vec3(0.0);
    for (int i = 0; i < numSpotLights; ++i)
        sum += entitySpotLightContribOne(spotLights[i], fragPosRel, normal);
    return sum;
}

// Final lit color. Mirrors lighting.frag's CalcDirLight (without specular):
//   ambient *= (1 - shadow*0.5)   [shadowed-ambient floor - see lighting.frag:443]
//   diffuse *= (1 - shadow)
// Then applies sky-light modulation so caves
// darken entities the same way they darken terrain.
//
// `skyFactor` is 0..1 (0 = pitch-black cave, 1 = open sky). The caller decides
// where it comes from: mobs/players read a per-entity uniform; dropped items
// read a per-vertex attribute (since they're rendered in a single batched draw
// call, a uniform can't distinguish per-item values).
// blockFactor (0..1) is baked torch block-light sampled at the entity, added
// as a warm emissive term NOT modulated by sky/shadow so a torch lights mobs
// / players / items in a pitch-black cave (matches lighting.frag's terrain
// torch term). 4-arg overload keeps existing callers working (blockFactor=0).
vec3 entityLitColor(vec3 albedo, vec3 normal, vec3 fragPosRel,
                    float skyFactor, float blockFactor) {
    vec3 lightDir = normalize(-dirLight.direction);
    float ndotl = max(dot(normal, lightDir), 0.0);
    float shadow = entityCSMShadow(fragPosRel, normal, lightDir);

    vec3 ambient = dirLight.ambient * (1.0 - shadow * 0.5);
    vec3 diffuse = dirLight.diffuse * ndotl * (1.0 - shadow);

    // Sky-light modulation. MIN_CAVE_LIGHT must match lighting.frag.
    const float MIN_CAVE_LIGHT = 0.04;
    ambient *= max(skyFactor, MIN_CAVE_LIGHT);
    diffuse *= skyFactor;

    vec3 points  = entityPointLightsContrib(fragPosRel, normal);
    vec3 spot    = entitySpotLightContrib(fragPosRel, normal);
    vec3 torch   = vec3(1.0, 0.62, 0.30) * (blockFactor * blockFactor) * 1.6;
    return albedo * (ambient + diffuse + points + spot + torch);
}

vec3 entityLitColor(vec3 albedo, vec3 normal, vec3 fragPosRel, float skyFactor) {
    return entityLitColor(albedo, normal, fragPosRel, skyFactor, 0.0);
}
