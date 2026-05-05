#version 460 core

in vec4 clipSpace;
in vec3 toCameraVector;
in vec2 textureCoords;

out vec4 FragColor;

uniform sampler2D reflectionTexture;
uniform sampler2D refractionTexture;
uniform sampler2D dudvMap;
uniform sampler2D normalMap;
uniform sampler2D refractionDepthTexture;

uniform float moveFactor;
uniform vec3 lightColor;
// Sun-elevation thresholds, expressed as components of sunDir.y in [-1, 1].
// Specular fades smoothly from 0 at twilightLow up to 1 at twilightHigh.
uniform float twilightLow;
uniform float twilightHigh;
uniform float nearPlane;
uniform float farPlane;

// Water properties
uniform float waveStrength;        // Distortion intensity
const float shineDamper = 20.0;
const float reflectivity = 0.5;

// Distance fog (sky LUT blending)
uniform sampler2D skyLUT;
uniform float skyExposure;
uniform float fogStart;
uniform float fogEnd;
uniform float fogStrength;
uniform bool fogEnabled;
uniform vec3 sunDir;
uniform float fogMieG;
uniform float fogMieStrength;

#include "sky_common.glsl"

void main() {
    vec2 ndc = (clipSpace.xy/clipSpace.w) * 0.5 + 0.5;
    vec2 refractTexCoords = vec2(ndc.x, ndc.y);
    vec2 reflectTexCoords = vec2(ndc.x, 1.0 - ndc.y);

    float depth = texture(refractionDepthTexture, refractTexCoords).r;
    float floorDistance = 2.0 * nearPlane * farPlane / (farPlane + nearPlane - (2.0 * depth - 1.0) * (farPlane - nearPlane));

    depth = gl_FragCoord.z;
    float waterDistance = 2.0 * nearPlane * farPlane / (farPlane + nearPlane - (2.0 * depth - 1.0) * (farPlane - nearPlane));
    float waterDepth = floorDistance - waterDistance;

    vec2 distortedTexCoords = texture(dudvMap, vec2(textureCoords.x + moveFactor, textureCoords.y)).rg * 0.1;
    distortedTexCoords = textureCoords + vec2(distortedTexCoords.x, distortedTexCoords.y + moveFactor);
    vec2 totalDistortion = (texture(dudvMap, distortedTexCoords).rg * 2.0 - 1.0) * waveStrength * clamp(waterDepth/20.0, 0.0, 1.0);

    refractTexCoords += totalDistortion;
    refractTexCoords = clamp(refractTexCoords, 0.001, 0.999);

    reflectTexCoords += totalDistortion;

    vec4 waterColor = vec4(0.0, 0.3, 0.5, 1.0);
    vec4 murkyWaterColor = vec4(0.0, 0.5, 0.275, 1.0);

    vec4 reflectColor = texture(reflectionTexture, reflectTexCoords);
    vec4 refractColor = texture(refractionTexture, refractTexCoords);
    refractColor = mix(refractColor, murkyWaterColor, clamp(waterDepth/60.0, 0.0, 1.0));

    // Normal calculations
    vec4 normalMapColor = texture(normalMap, distortedTexCoords);
    vec3 normal = vec3(normalMapColor.r * 2.0 - 1.0, normalMapColor.b * 3.0, normalMapColor.g * 2.0 - 1.0);
    normal = normalize(normal);

    // Fresnel calculation
    vec3 viewVector = normalize(toCameraVector);
    float refractiveFactor = dot(viewVector, normal);
    refractiveFactor = pow(refractiveFactor, 1.0); // The higer the value the more reflective when looking at an angle
    refractiveFactor = clamp(refractiveFactor, 0.001, 0.999);

    // Light reflection calculation. Sun is a directional light: sunDir points
    // toward the sun, so the incoming light direction is -sunDir. Using the
    // direction directly (instead of reconstructing it from a worldspace
    // light position) is what keeps specular correct at large coordinates.
    vec3 reflectedLight = reflect(-sunDir, normal);
    float specular = max(dot(reflectedLight, viewVector), 0.0);
    specular = pow(specular, shineDamper);
    vec3 specularHighlights = lightColor * specular * reflectivity * clamp(waterDepth/5.0, 0.0, 1.0);

    // Smoothly fade specular highlights around the horizon based on sun elevation.
    float dayFactor = smoothstep(twilightLow, twilightHigh, sunDir.y);
    specularHighlights *= dayFactor;

    FragColor = mix(reflectColor, refractColor, refractiveFactor);
    FragColor = mix(FragColor, waterColor, 0.2) + vec4(specularHighlights, 0.0); // water blue tint
    FragColor.a = clamp(waterDepth/5.0, 0.0, 1.0); // softens the edges of the water
//    FragColor = normalMapColor;
//    FragColor = vec4(waterDepth/50.0);

	if (!gl_FrontFacing) {
		// draw the inside with transparency
		FragColor.a *= 0.8;
	}

    if (fogEnabled) {
        float dist = length(toCameraVector);
        float fogFactor = 1.0 - pow(smoothstep(fogStart, fogEnd, dist), fogStrength);
        vec3 viewDir = normalize(-toCameraVector); // direction from camera toward water
        FragColor.rgb = applyDistanceFog(FragColor.rgb, viewDir, sunDir,
                                         skyLUT, skyExposure,
                                         fogStart, fogEnd, fogStrength,
                                         dist, fogMieG, fogMieStrength);
        // Also fade alpha so water edge softens into fog
        FragColor.a *= fogFactor;
    }
}