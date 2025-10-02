#version 460 core

in vec2 TexCoord;
in vec4 clipSpace;
in vec3 toCameraVector;
in vec3 fromLightVector;

out vec4 FragColor;

uniform sampler2D reflectionTexture;
uniform sampler2D refractionTexture;
uniform sampler2D dudvMap;
uniform sampler2D normalMap;
uniform sampler2D refractionDepthTexture;

uniform float moveFactor;
uniform vec3 lightColor;
uniform vec3 lightPos;

// Water properties
const float waveStrength = 0.02;        // Distortion intensity
const float shineDamper = 20.0;         // Specular shininess
const float reflectivity = 0.6;         // Specular reflectivity
const float waterTint = 0.15;           // Blue tint strength

void main() {
    // Convert clip space to normalized device coordinates [-1, 1]
    vec2 ndc = (clipSpace.xy / clipSpace.w);
    
    // Convert to texture coordinates [0, 1]
    vec2 refractTexCoords = (ndc + 1.0) * 0.5;
    vec2 reflectTexCoords = vec2(refractTexCoords.x, 1.0 - refractTexCoords.y);
    
    // Sample depth to avoid edge artifacts
    float depth = texture(refractionDepthTexture, refractTexCoords).r;
    float floorDistance = 2.0 * 0.1 * 1000.0 / (1000.0 + 0.1 - (2.0 * depth - 1.0) * (1000.0 - 0.1));
    depth = gl_FragCoord.z;
    float waterDistance = 2.0 * 0.1 * 1000.0 / (1000.0 + 0.1 - (2.0 * depth - 1.0) * (1000.0 - 0.1));
    float waterDepth = floorDistance - waterDistance;
    
    // Sample DuDv map for distortion (with time-based offset)
    //vec2 distortedTexCoords = texture(dudvMap, vec2(TexCoord.x + moveFactor, TexCoord.y)).rg * 0.1;
    //distortedTexCoords = TexCoord + vec2(distortedTexCoords.x, distortedTexCoords.y + moveFactor);
    //vec2 totalDistortion = (texture(dudvMap, distortedTexCoords).rg * 2.0 - 1.0) * waveStrength * clamp(waterDepth / 20.0, 0.0, 1.0);
    
    // Apply distortion to texture coordinates
    refractTexCoords = clamp(refractTexCoords , 0.001, 0.999);
    reflectTexCoords = clamp(reflectTexCoords , 0.001, 0.999);
    
    // Sample reflection and refraction textures
    vec4 reflectColor = texture(reflectionTexture, reflectTexCoords);
    vec4 refractColor = texture(refractionTexture, refractTexCoords);
    
    // Sample normal map and convert from [0,1] to [-1,1]
    //vec4 normalMapColor = texture(normalMap, distortedTexCoords);
    //vec3 normal = vec3(normalMapColor.r * 2.0 - 1.0, normalMapColor.b * 3.0, normalMapColor.g * 2.0 - 1.0);
    //normal = normalize(normal);
    
    // Calculate view direction for Fresnel effect
    //vec3 viewVector = normalize(toCameraVector);
    //float refractiveFactor = viewVector;
    //refractiveFactor = pow(refractiveFactor, 0.5);  // Fresnel effect
    //refractiveFactor = clamp(refractiveFactor, 0.0, 1.0);
    
    // Calculate specular lighting (sun reflection on water)
    //vec3 reflectedLight = reflect(normalize(fromLightVector), normal);
    //float specular = max(dot(reflectedLight, viewVector), 0.0);
    //specular = pow(specular, shineDamper);
    //vec3 specularHighlights = lightColor * specular * reflectivity * clamp(waterDepth / 5.0, 0.0, 1.0);
    
    // Mix reflection and refraction based on Fresnel
    FragColor = mix(reflectColor, refractColor, 1.0);
    
    // Add blue tint to water
    FragColor = mix(FragColor, vec4(0.0, 0.3, 0.5, 1.0), waterTint);
    
    // Add specular highlights
    FragColor = vec4(FragColor.rgb, 1.0);
    
    // Add slight transparency based on depth
    FragColor.a = clamp(waterDepth / 5.0, 0.0, 1.0);
}
