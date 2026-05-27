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
uniform float moveFactor2;
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
uniform bool hdrMode; // HDR: keep fog linear, tonemap once at the end.
uniform vec3 sunDir;
uniform vec3 underwaterFogColor;

#include "sky_common.glsl"

vec2 sampleDistortion(vec2 baseUV) {
    vec2 uv1 = vec2(baseUV.x + moveFactor,  baseUV.y                );
    vec2 uv2 = vec2(baseUV.x - moveFactor2, baseUV.y + moveFactor2);
    vec2 d1 = texture(dudvMap, uv1       ).rg * 0.1;
    vec2 d2 = texture(dudvMap, uv2 * 4.0 ).rg * 0.1;
    vec2 c1 = baseUV       + vec2(d1.x, d1.y + moveFactor );
    vec2 c2 = baseUV * 4.0 + vec2(d2.x, d2.y - moveFactor2);
    return ((texture(dudvMap, c1).rg * 2.0 - 1.0)
          + (texture(dudvMap, c2).rg * 2.0 - 1.0) * 0.5);
}

// Two-tap normal map sampling at different scales - the small-scale layer
// adds high-frequency ripple detail without retiling the big waves.
vec3 sampleNormal(vec2 distortedUV) {
    vec4 n1 = texture(normalMap, distortedUV);
    vec4 n2 = texture(normalMap, distortedUV * 4.0 + vec2(moveFactor2, -moveFactor2));
    vec3 N1 = vec3(n1.r * 2.0 - 1.0, n1.b * 3.0, n1.g * 2.0 - 1.0);
    vec3 N2 = vec3(n2.r * 2.0 - 1.0, n2.b * 3.0, n2.g * 2.0 - 1.0);
    return normalize(N1 + N2 * 0.5);
}

void main() {
    vec2 ndc = (clipSpace.xy/clipSpace.w) * 0.5 + 0.5;
    vec2 refractTexCoords = vec2(ndc.x, ndc.y);
    vec2 reflectTexCoords = vec2(ndc.x, 1.0 - ndc.y);

    float depth = texture(refractionDepthTexture, refractTexCoords).r;
    float floorDistance = 2.0 * nearPlane * farPlane / (farPlane + nearPlane - (2.0 * depth - 1.0) * (farPlane - nearPlane));

    depth = gl_FragCoord.z;
    float waterDistance = 2.0 * nearPlane * farPlane / (farPlane + nearPlane - (2.0 * depth - 1.0) * (farPlane - nearPlane));
    float waterDepth = floorDistance - waterDistance;

    // Multi-octave dudv distortion
    vec2 totalDistortion = sampleDistortion(textureCoords) * waveStrength * clamp(waterDepth/20.0, 0.0, 1.0);

    // Refraction also bends through the rippled surface - use a smaller
    // factor so the underwater silhouette stays readable while still
    // tracking the surface dudv, then clamp to keep the sample inside the
    // refraction FBO (otherwise we'd read the above-water clipped region).
    refractTexCoords += totalDistortion * 0.5;
    refractTexCoords = clamp(refractTexCoords, 0.001, 0.999);

    reflectTexCoords += totalDistortion;

   // underwater ceiling
   if (!gl_FrontFacing) {
        vec3 normalUW = sampleNormal(textureCoords
            + sampleDistortion(textureCoords) * waveStrength);
        vec3 viewUW = normalize(toCameraVector);

        // Sun glint coming through the surface
        vec3 reflLight = reflect(-sunDir, normalUW);
        float glint = pow(max(dot(reflLight, viewUW), 0.0), 80.0);
        float dayFactor = smoothstep(twilightLow, twilightHigh, sunDir.y);

        // Shift the base toward a dedicated night color so the surface
        // tracks the day/night cycle
        float dayT = smoothstep(-0.1, 0.2, sunDir.y);
        vec3 baseColor = mix(vec3(0.0, 0.096, 0.085), underwaterFogColor, dayT);
        vec3 col = baseColor + lightColor * glint * 2.0 * dayFactor;

        // Thin near the eye (see through it), opaque far away (merges with
        // the fog). length(toCameraVector) is the distance to this fragment.
        float d = length(toCameraVector);
        float a = mix(0.12, 0.85, smoothstep(2.0, 35.0, d));

        FragColor = vec4(col, a);
        return;
    }

    vec4 waterColor = vec4(0.0, 0.3, 0.5, 1.0);
    vec4 murkyWaterColor = vec4(0.0, 0.5, 0.275, 1.0);

    vec4 reflectColor = texture(reflectionTexture, reflectTexCoords);
    vec4 refractColor = texture(refractionTexture, refractTexCoords);
    refractColor = mix(refractColor, murkyWaterColor, clamp(waterDepth/60.0, 0.0, 1.0));

    // Multi-octave normal - see sampleNormal().
    // Use the *first* sampled UV (consistent with old single-tap behavior) for
    // the distortion that drives normals.
    vec2 distortedTexCoords = textureCoords + totalDistortion;
    vec3 normal = sampleNormal(distortedTexCoords);

    // Fresnel
    vec3 viewVector = normalize(toCameraVector);
    float refractiveFactor = clamp(dot(viewVector, normal), 0.001, 0.999);

    // Soft Phong specular (existing wide highlight)
    vec3 reflectedLight = reflect(-sunDir, normal);
    float specSoft = pow(max(dot(reflectedLight, viewVector), 0.0), shineDamper);
    // Sharp sun-disk specular: a much narrower lobe stacked on top of the
    // soft one. This is the bright glint you see catching the sun on water.
    float specSharp = pow(max(dot(reflectedLight, viewVector), 0.0), 200.0);
    vec3 specularHighlights = lightColor * (specSoft * reflectivity + specSharp * 1.5)
                              * clamp(waterDepth/5.0, 0.0, 1.0);

    // Twilight fade
    float dayFactor = smoothstep(twilightLow, twilightHigh, sunDir.y);
    specularHighlights *= dayFactor;

    FragColor = mix(reflectColor, refractColor, refractiveFactor);
    FragColor = mix(FragColor, waterColor, 0.2) + vec4(specularHighlights, 0.0); // water blue tint

    FragColor.a = clamp(waterDepth/5.0, 0.0, 1.0); // softens the edges of the water
//    FragColor = normalMapColor;
//    FragColor = vec4(waterDepth/50.0);

    // Distance-based color shift: real water turns sky-blue at distance.
    // Mix toward the sky color sampled in the view direction. This is on top
    // of any fog blend below - the two together give a smooth horizon.
    float viewDist = length(toCameraVector);
    {
        vec3 viewDirToFrag = normalize(-toCameraVector);
        vec3 horizonColor = sampleSkyColor(skyLUT, viewDirToFrag, sunDir, skyExposure);
        float horizonMix = smoothstep(40.0, 250.0, viewDist) * 0.35;
        FragColor.rgb = mix(FragColor.rgb, horizonColor, horizonMix);
    }

    if (fogEnabled) {
        float fogFactor = 1.0 - pow(smoothstep(fogStart, fogEnd, viewDist), fogStrength);
        vec3 viewDir = normalize(-toCameraVector); // direction from camera toward water
        vec3 fogColor = hdrMode
            ? sampleSkyColorLinear(skyLUT, viewDir, sunDir)
            : sampleSkyColor(skyLUT, viewDir, sunDir, skyExposure);
        FragColor.rgb = mix(fogColor, FragColor.rgb, fogFactor);
        // Also fade alpha so water edge softens into fog
        FragColor.a *= fogFactor;
    }
}
