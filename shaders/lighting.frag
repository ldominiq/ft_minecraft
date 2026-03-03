#version 460 core

in VS_OUT {
    vec3 FragPos;
    vec3 Normal;
    vec2 TexCoord;
    float SkyLight; // 0.0 = fully underground, 1.0 = open sky
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

uniform bool debugCascades;   // toggle from ImGui
int debugCascadeLayer = -1;   // set by CSMShadowCalculation

uniform sampler2D atlas;
uniform DirLight dirLight;
uniform PointLight pointLights[NR_POINT_LIGHTS];
uniform SpotLight spotLight;
uniform Material material;

uniform sampler2D diffuseTexture;

uniform vec3 lightPos;
uniform vec3 viewPos;
uniform int renderType;
uniform bool blinn;

uniform Shadows shadows;

// CSM uniforms
uniform sampler2DArray shadowMapArray;
uniform mat4 lightSpaceMatrices[MAX_CASCADES];
uniform float cascadePlaneDistances[MAX_CASCADES - 1]; // N-1 split points for N cascades
uniform int cascadeCount;
uniform float farPlane;
uniform mat4 view;

// SSAO
uniform sampler2D ssaoTexture;
uniform int ssaoEnabled;
uniform vec2 screenSize; // Full viewport resolution for correct SSAO UV mapping

float near = 0.1;
float far  = 100.0;

float LinearizeDepth(float depth)
{
    float z = depth * 2.0 - 1.0; // back to NDC
    return (2.0 * near * far) / (far + near - z * (far - near));
}

// function prototypes
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, float ao);
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, float ao);
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, float ao);
float CSMShadowCalculation(vec3 fragPosWorldSpace);

void main()
{    
    // properties
    vec3 color = texture(diffuseTexture, fs_in.TexCoord).rgb;
    vec3 norm = normalize(fs_in.Normal);
    vec3 viewDir = normalize(viewPos - fs_in.FragPos);
    
    // SSAO
    float AmbientOcclusion = 1.0;
    if (ssaoEnabled == 1) {
        // Use full viewport size, not texture size — the SSAO texture may
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
    vec3 result = CalcDirLight(dirLight, norm, viewDir, AmbientOcclusion);
    // phase 2: point lights
    for(int i = 0; i < NR_POINT_LIGHTS; i++)
        result += CalcPointLight(pointLights[i], norm, fs_in.FragPos, viewDir, AmbientOcclusion);    
    // phase 3: spot light
    result += CalcSpotLight(spotLight, norm, fs_in.FragPos, viewDir, AmbientOcclusion);    

    if (renderType == 1) {
        FragColor = vec4(norm * 0.5 + 0.5, 1.0); // Visualize normals
    }
    else if (renderType == 2) {
        float depth = LinearizeDepth(gl_FragCoord.z) / far; // Visualize depth buffer
        FragColor = vec4(vec3(depth), 1.0);

    } else {
        FragColor = vec4(result * color, 1.0);
    }

    // ── Cascade debug overlay ──
    // Tints each cascade a different color so you can see the boundaries
    if (debugCascades && debugCascadeLayer >= 0)
    {
        vec3 cascadeColor;
        if (debugCascadeLayer == 0)
            cascadeColor = vec3(1.0, 0.0, 0.0);  // Red   — closest
        else if (debugCascadeLayer == 1)
            cascadeColor = vec3(0.0, 1.0, 0.0);  // Green
        else if (debugCascadeLayer == 2)
            cascadeColor = vec3(0.0, 0.0, 1.0);  // Blue
        else if (debugCascadeLayer == 3)
            cascadeColor = vec3(1.0, 1.0, 0.0);  // Yellow
        else
            cascadeColor = vec3(1.0, 0.0, 1.0);  // Magenta — farthest

        // Mix: 80% original color + 20% cascade tint
        FragColor = vec4(mix(FragColor.rgb, cascadeColor, 0.2), FragColor.a);
    }

    //FragColor = vec4(lighting, texColor.a); // Lighting
    //FragColor = vec4(fs_in.TexCoord, 0.0, 1.0); // Visualize texture coordinates
}

float CSMShadowCalculation(vec3 fragPosWorldSpace)
{
    if (cascadeCount == 0)
    {
        return 0.0;
    }   

    // 1. Find fragment depth in VIEW SPACE.
    //    We need to know how far this fragment is from the camera
    //    so we can pick the right cascade.
    vec4 fragPosViewSpace = view * vec4(fragPosWorldSpace, 1.0);
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

    // 3. Project fragment into the selected cascade's light space.
    vec4 fragPosLightSpace = lightSpaceMatrices[layer] * vec4(fragPosWorldSpace, 1.0);

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
    {
        return 0.0;
    }

    float currentDepth = projCoords.z;

    // 5. Bias — scale proportional to the texel size of this cascade.
    //    Larger cascades cover more world space per texel, so they
    //    need proportionally more bias.  We derive the scale from the
    //    shadow map resolution vs the cascade's projected extent (which
    //    is encoded implicitly in the texel size of the projCoords).
    vec3 normal = normalize(fs_in.Normal);
    vec3 lightDir = normalize(-dirLight.direction);
    float ndotl = max(dot(normal, lightDir), 0.0);
    float baseBias = max(shadows.MAX_BIAS * (1.0 - ndotl), shadows.MIN_BIAS);

    // Each successive cascade covers roughly 4× the area of the previous,
    // so texel size doubles.  Scale bias accordingly.
    float cascadeScale = 1.0 + float(layer) * 0.5;
    float bias = baseBias * cascadeScale;

    // 6. PCF (Percentage Closer Filtering).
    //    Sample neighboring texels for softer shadow edges.
    float shadow = 0.0;
    vec2 texelSize = 1.0 / vec2(textureSize(shadowMapArray, 0));

    // Use a larger PCF kernel for far cascades where individual
    // texels cover more world space (reduces blockiness).
    int pcfRadius = 1 + layer;  // cascade 0→3×3, 1→5×5, 2→7×7
    float sampleCount = 0.0;
    for (int x = -pcfRadius; x <= pcfRadius; ++x)
    {
        for (int y = -pcfRadius; y <= pcfRadius; ++y)
        {
            float pcfDepth = texture(
                shadowMapArray,
                vec3(projCoords.xy + vec2(x, y) * texelSize, layer)
            ).r;
            shadow += (currentDepth - bias) > pcfDepth ? 1.0 : 0.0;
            sampleCount += 1.0;
        }
    }
    shadow /= sampleCount;

    // 7. Fade shadow at the edge of the last cascade to avoid hard cutoff.
    float maxDist = (layer == cascadeCount - 1) ? farPlane : cascadePlaneDistances[layer];
    float fadeStart = maxDist * 0.9;
    if (depthValue > fadeStart && layer == cascadeCount - 1)
    {
        float t = (depthValue - fadeStart) / (maxDist - fadeStart);
        shadow *= 1.0 - clamp(t, 0.0, 1.0);
    }

    return shadow;
}

// calculates the color when using a directional light.
vec3 CalcDirLight(DirLight light, vec3 normal, vec3 viewDir, float ao)
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
    vec3 ambient = light.ambient * vec3(texture(atlas, fs_in.TexCoord));
    vec3 diffuse = light.diffuse * diff * vec3(texture(atlas, fs_in.TexCoord));
    vec3 specular = light.specular * spec * vec3(texture(atlas, fs_in.TexCoord));

    // calculate shadow
    float shadow = 0.0;
    if (shadows.enabled)
        if (light.direction.y < 0.0)
        {
            shadow = CSMShadowCalculation(fs_in.FragPos);
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
    // would still receive full diffuse/specular — looking bright
    // underground.  skyFactor tells us the block is enclosed, so
    // direct sunlight shouldn't reach it regardless of the shadow
    // map's opinion.
    diffuse  *= skyFactor;
    specular *= skyFactor;

    // Also darken ambient in shadowed areas.  Without this, faces
    // behind hills/terrain at sunset still glow because diffuse is
    // near zero (low sun angle) and ambient bypasses the shadow map.
    // We blend: in shadow, ambient drops to 30% of its value.
    float ambientShadowFactor = 1.0 - shadow * 0.7;
    ambient *= ambientShadowFactor;

    // SSAO: darken ambient by screen-space occlusion
    ambient *= ao;

    vec3 lighting = (ambient + (1.0 - shadow) * (diffuse + specular));    
    return (lighting);
}

// calculates the color when using a point light.
vec3 CalcPointLight(PointLight light, vec3 normal, vec3 fragPos, vec3 viewDir, float ao)
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
    vec3 ambient = light.ambient * vec3(texture(atlas, fs_in.TexCoord));
    vec3 diffuse = light.diffuse * diff * vec3(texture(atlas, fs_in.TexCoord));
    vec3 specular = light.specular * spec * vec3(texture(atlas, fs_in.TexCoord));
    ambient *= attenuation;
    diffuse *= attenuation;
    specular *= attenuation;
    ambient *= ao;
    return (ambient + diffuse + specular);
}

// calculates the color when using a spot light.
vec3 CalcSpotLight(SpotLight light, vec3 normal, vec3 fragPos, vec3 viewDir, float ao)
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
    vec3 ambient = light.ambient * vec3(texture(atlas, fs_in.TexCoord));
    vec3 diffuse = light.diffuse * diff * vec3(texture(atlas, fs_in.TexCoord));
    vec3 specular = light.specular * spec * vec3(texture(atlas, fs_in.TexCoord));
    ambient *= attenuation * intensity;
    diffuse *= attenuation * intensity;
    specular *= attenuation * intensity;
    ambient *= ao;
    return (ambient + diffuse + specular);
}
