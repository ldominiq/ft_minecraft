#version 460 core

in vec4 clipSpace;
in vec3 toCameraVector;
in vec3 worldPos;
in vec2 textureCoords;

out vec4 FragColor;

uniform sampler2D reflectionTexture;
uniform sampler2D refractionTexture;
uniform sampler2D dudvMap;
//uniform sampler2D normalMap;
//uniform sampler2D refractionDepthTexture;
//
uniform float moveFactor;
//uniform vec3 lightColor;
//uniform float nearPlane;
//uniform float farPlane;
//uniform vec3 sunDir;
//uniform float seaLevel;
//
//// Water properties
uniform float waveStrength;        // Distortion intensity
//const float reflectivity = 0.6;         // Specular reflectivity
//const float shine = 32.0f;              // sharper Blinn-phong
//const float F0 = 0.02;                 // water base reflection
//const float specScale = 0.25;
//
//// Adjust overall texture scale (bigger = fewer repeats over the world)
//const float dudvScale1 = 0.025; // ~ 40 world units per repeat
//const float dudvScale2 = 0.010; // larger scale second octave
//const float normalScale = 0.020;
//
//float linearizeDepth(const float depth, const float nearP, const float farP) {
//    float z = depth * 2.0 - 1.0;
//    return (2.0 * nearP * farP) / (farP + nearP - z * (farP - nearP));
//}

void main() {
    // Screen UV from clip space
//    vec2 ndc = (clipSpace.xy / clipSpace.w) / 2.0 + 0.5;
//    vec2 uvRefl = vec2(ndc.x, 1.0 - ndc.y); // Y flipped
//    vec2 uvRefr = ndc;
//
//    // Depth based metrics
//    float sceneDepth = texture(refractionDepthTexture, uvRefr).r;
//    float floorDist = linearizeDepth(sceneDepth, nearPlane, farPlane);
//    float waterDist = linearizeDepth(gl_FragCoord.z, nearPlane, farPlane);
//    float waterDepth = max(floorDist - waterDist, 0.0);
//    float depthFactor = clamp(waterDepth / 5.0, 0.0, 1.0); // 5m to full depth
//
//
//    // Two scrolling layers with different scales and speeds to break repetition
//    float t = fract(moveFactor);
//    vec2 uv1 = worldPos.xz * dudvScale1 + vec2( t, 0.0);
//    vec2 uv2 = worldPos.xz * dudvScale2 + vec2(-t * 0.5, 0.2 * t);
//
//    vec2 dudv1 = texture(dudvMap, uv1).rg;
//    vec2 dudv2 = texture(dudvMap, uv2).rg;
//    vec2 dudv  = mix(dudv1, dudv2, 0.45) * 2.0 - 1.0;
//
//    float distortionScale = mix(0.0, 1.0, depthFactor);
//    vec2 distortion = dudv * waveStrength * distortionScale;
//
//    // Apply distortion only to reflection/refraction UVs (screen-space)
//    uvRefl = clamp(uvRefl + distortion, vec2(0.001), vec2(0.999));
//    uvRefr = clamp(uvRefr + distortion, vec2(0.001), vec2(0.999));
//
//    // Sample reflection and refraction after applying distortion
//    vec3 refl = texture(reflectionTexture, uvRefl).rgb;
//    vec3 refr = texture(refractionTexture, uvRefr).rgb;
//
//    // For normal map, also use world-space UVs (with a single consistent scale)
//    vec2 nUV = worldPos.xz * normalScale + distortion;  // slight distortion ok
//    vec3 nTex = texture(normalMap, nUV).rgb;
//    vec3 nTan = nTex * 2.0 - 1.0;
//    vec3 N = normalize(vec3(nTan.x, nTan.z, nTan.y));
//
//    // Fresnel (Schlick)
//    vec3 V = normalize(toCameraVector);
//    float cosTheta = clamp(dot(N, V), 0.0, 1.0);
//    float F = F0 + (1.0 - F0) * pow(1.0 - cosTheta, 5.0);
//    F = clamp(F, 0.02, 0.95);
//
//    // Specular (Blinn-phong)
//    vec3 L = normalize(-sunDir);
//    float NdotL = max(dot(N, L), 0.0);
//    vec3 H = normalize(L + V);
//    float spec = (NdotL > 0.0) ? pow(max(dot(N, H), 0.0), shine) : 0.0;
//    vec3 specCol = lightColor * (spec * specScale); // test different scale
//
//    vec3 diffuse = lightColor * (0.05 * NdotL);
//
//    // Attenuate reflections for non sea-level water blocks to avoid incorrect global planar reflection
//    float seaDelta = abs(worldPos.y - seaLevel);
//    float seaBlend = 1.0 - smoothstep(0.0, 0.75, seaDelta); // within ~0.75 units of sea level => full reflection
//    float reflectWeight = F * seaBlend;
//
//    // Mix reflect/refract
//    vec3 color = mix(refr, refl, reflectWeight);
//
//    // Depth-based tint
//    vec3 shallowColor = vec3(0.0, 0.45, 0.65);
//    vec3 deepColor = vec3(0.0, 0.08, 0.15);
//    vec3 tint = mix(shallowColor, deepColor, depthFactor);
//    color = mix(color, tint, 0.35 * depthFactor);
//
//    // Alpha combine Fresnel and depth
//    float alphaFacing  = 0.60;
//    float alphaGrazing = 0.85;
//    float alpha = mix(alphaGrazing, alphaFacing, 1.0 - F); // Fresnel-driven
//    alpha = mix(alpha, 0.92, depthFactor);                  // deeper → more opaque
//
//    color += diffuse;
//    color += specCol;

//    FragColor = vec4(color, alpha);

    vec2 ndc = (clipSpace.xy/clipSpace.w) * 0.5 + 0.5;
    vec2 refractTexCoords = vec2(ndc.x, ndc.y);
    vec2 reflectTexCoords = vec2(ndc.x, 1.0 - ndc.y);

    vec2 distortion1 = (texture(dudvMap, vec2(textureCoords.x + moveFactor, textureCoords.y)).rg * 2.0 - 1.0) * waveStrength;
    vec2 distortion2 = (texture(dudvMap, vec2(-textureCoords.x + moveFactor, textureCoords.y + moveFactor)).rg * 2.0 - 1.0) * waveStrength;
    vec2 totalDistortion = distortion1 + distortion2;

    refractTexCoords += totalDistortion;
    refractTexCoords = clamp(refractTexCoords, 0.001, 0.999);

    reflectTexCoords += totalDistortion;
//    reflectTexCoords.x = clamp(reflectTexCoords.x, 0.001, 0.999);
//    reflectTexCoords.y = clamp(reflectTexCoords.y, -0.999, -0.001);

    vec4 reflectColor = texture(reflectionTexture, reflectTexCoords);
    vec4 refractColor = texture(refractionTexture, refractTexCoords);


    FragColor = mix(reflectColor, refractColor, 0.5);
    FragColor = mix(FragColor, vec4(0.0, 0.3, 0.5, 1.0), 0.2);
}