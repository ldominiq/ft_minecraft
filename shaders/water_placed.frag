#version 460 core

// Shading path for water that doesn't sit on the global sea-level plane
// (placed buckets, spreading water, exposed ocean cliff sides). The planar
// reflection texture is mirrored across y == seaLevel and the refraction
// texture is clipped at y == seaLevel, so neither of them is meaningful for
// this geometry. Instead we read reflection from the sky LUT and treat the
// "underwater" term as a flat blue tint — nothing here depends on the
// fragment's world Y, so it shades correctly at any height.

in vec4 clipSpace;
in vec3 toCameraVector;
in vec2 textureCoords;

out vec4 FragColor;

uniform sampler2D dudvMap;
uniform sampler2D normalMap;

uniform float moveFactor;
uniform vec3 lightColor;
// Sun-elevation thresholds (matching ocean shader): specular fades smoothly
// from 0 at twilightLow up to 1 at twilightHigh.
uniform float twilightLow;
uniform float twilightHigh;

uniform float waveStrength;
const float shineDamper = 20.0;
const float reflectivity = 0.5;

uniform sampler2D skyLUT;
uniform float skyExposure;
uniform float fogStart;
uniform float fogEnd;
uniform float fogStrength;
uniform bool fogEnabled;
uniform vec3 sunDir;

#include "sky_common.glsl"

void main() {
    // DUDV / normal-map distortion — same two-pass scheme as the ocean
    // shader, minus the depth-based shallow-water dampening.
    vec2 distortedTexCoords = texture(dudvMap, vec2(textureCoords.x + moveFactor, textureCoords.y)).rg * 0.1;
    distortedTexCoords = textureCoords + vec2(distortedTexCoords.x, distortedTexCoords.y + moveFactor);
    vec2 totalDistortion = (texture(dudvMap, distortedTexCoords).rg * 2.0 - 1.0) * waveStrength;

    vec4 normalMapColor = texture(normalMap, distortedTexCoords);
    vec3 normal = vec3(normalMapColor.r * 2.0 - 1.0, normalMapColor.b * 3.0, normalMapColor.g * 2.0 - 1.0);
    normal = normalize(normal);

    // View-incoming direction is camera->fragment. toCameraVector points
    // fragment->camera, so negate to flip it.
    vec3 viewVector = normalize(toCameraVector);
    vec3 viewIncoming = -viewVector;

    // Reflection: bounce the incoming view ray off the perturbed water
    // normal, then sample the sky LUT in that direction. This is what
    // gives placed water a believable sky/sun reflection without any
    // dependence on the planar reflection texture.
    vec3 reflectDir = reflect(viewIncoming, normal);
    // Subtle distortion on top of the geometric reflection — keeps small
    // ripples lively even when looking straight down (where reflectDir is
    // almost vertical and barely changes from the normal perturbation).
    reflectDir.xz += totalDistortion;
    reflectDir = normalize(reflectDir);
    vec3 reflectColor = sampleSkyColor(skyLUT, reflectDir, sunDir, skyExposure);

    // "Refraction" term: no underlying scene texture; just the same blue
    // we'd otherwise see through deep water. Cheap, plane-independent.
    vec3 waterColor = vec3(0.0, 0.3, 0.5);
    vec3 refractColor = waterColor;

    // Fresnel: more reflection at grazing angles, matching ocean shader.
    float refractiveFactor = clamp(dot(viewVector, normal), 0.001, 0.999);

    // Specular highlights from the sun. Same model as ocean; depth-based
    // shallow-water fade is dropped (we don't have water depth here).
    vec3 reflectedLight = reflect(-sunDir, normal);
    float specular = max(dot(reflectedLight, viewVector), 0.0);
    specular = pow(specular, shineDamper);
    vec3 specularHighlights = lightColor * specular * reflectivity;
    float dayFactor = smoothstep(twilightLow, twilightHigh, sunDir.y);
    specularHighlights *= dayFactor;

    vec3 col = mix(reflectColor, refractColor, refractiveFactor);
    col = mix(col, waterColor, 0.2) + specularHighlights;
    FragColor = vec4(col, 0.75);

    if (!gl_FrontFacing) {
        // Looking up through the surface — keep some translucency so caves
        // and pools don't read as opaque blocks.
        FragColor.a *= 0.8;
    }

    if (fogEnabled) {
        float dist = length(toCameraVector);
        float fogFactor = 1.0 - pow(smoothstep(fogStart, fogEnd, dist), fogStrength);
        vec3 viewDir = normalize(-toCameraVector); // camera toward fragment
        vec3 fogColor = sampleSkyColor(skyLUT, viewDir, sunDir, skyExposure);
        FragColor.rgb = mix(fogColor, FragColor.rgb, fogFactor);
        FragColor.a *= fogFactor;
    }
}
