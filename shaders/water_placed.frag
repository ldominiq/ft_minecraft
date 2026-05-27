#version 460 core

// Shading path for water that doesn't sit on the global sea-level plane
// (placed buckets, spreading water, exposed ocean cliff sides). The planar
// reflection texture is mirrored across y == seaLevel and the refraction
// texture is clipped at y == seaLevel, so neither of them is meaningful for
// this geometry. Instead we read reflection from the sky LUT and treat the
// "underwater" term as a flat blue tint - nothing here depends on the
// fragment's world Y, so it shades correctly at any height.

in vec4 clipSpace;
in vec3 toCameraVector;
in vec2 textureCoords;
// Vertex shader emits the cube-face index (0..5). We rebuild the face TBN
// from it so the normal map is interpreted as tangent-space rather than
// the +Y-up world space the ocean shader assumes.
flat in int faceNormalIdx;

out vec4 FragColor;

uniform sampler2D dudvMap;
uniform sampler2D normalMap;

uniform float moveFactor;
uniform float moveFactor2;
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
uniform vec3 underwaterFogColor;

#include "sky_common.glsl"

// Two-octave dudv - same idea as ocean shader. Each octave gets its own
// scroll (moveFactor / moveFactor2) so the GL_REPEAT wrap is integer-clean
// and the two layers drift apart over time without snapping.
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

// Tangent-space normal from two octaves of the dudv-distorted normal map.
// Channels follow the ocean shader's convention: r → tangent, g → bitangent,
// b → face normal (scaled to keep the perturbation weak relative to the
// geometric normal).
vec3 sampleTangentNormal(vec2 distortedUV) {
    vec4 n1 = texture(normalMap, distortedUV);
    vec4 n2 = texture(normalMap, distortedUV * 4.0 + vec2(moveFactor2, -moveFactor2));
    vec3 t1 = vec3(n1.r * 2.0 - 1.0, n1.g * 2.0 - 1.0, n1.b * 3.0);
    vec3 t2 = vec3(n2.r * 2.0 - 1.0, n2.g * 2.0 - 1.0, n2.b * 3.0);
    return normalize(t1 + t2 * 0.5);
}

// Axis-aligned TBN for one of the six cube faces. The U axis of the
// face-UV (see water_placed.vert) becomes T, V becomes B, and the outward
// face normal becomes N.
void getFaceTBN(int idx, out vec3 N, out vec3 T, out vec3 B) {
    if      (idx == 0) { N = vec3( 0, 0, 1); T = vec3( 1, 0, 0); B = vec3(0, 1, 0); }
    else if (idx == 1) { N = vec3( 0, 0,-1); T = vec3(-1, 0, 0); B = vec3(0, 1, 0); }
    else if (idx == 2) { N = vec3( 0, 1, 0); T = vec3( 1, 0, 0); B = vec3(0, 0, 1); }
    else if (idx == 3) { N = vec3( 0,-1, 0); T = vec3( 1, 0, 0); B = vec3(0, 0,-1); }
    else if (idx == 4) { N = vec3( 1, 0, 0); T = vec3( 0, 0,-1); B = vec3(0, 1, 0); }
    else               { N = vec3(-1, 0, 0); T = vec3( 0, 0, 1); B = vec3(0, 1, 0); }
}

void main() {
    vec2 totalDistortion = sampleDistortion(textureCoords) * waveStrength;
    vec2 distortedTexCoords = textureCoords + totalDistortion;

    // Build the face-local TBN, sample tangent-space normal, transform to
    // world space. For top faces this gives the same world normal the old
    // sampleNormal() returned directly; for side faces it now points along
    // the correct face axis instead of +Y.
    vec3 faceN, faceT, faceB;
    getFaceTBN(faceNormalIdx, faceN, faceT, faceB);
    vec3 nT = sampleTangentNormal(distortedTexCoords);
    vec3 normal = normalize(faceT * nT.x + faceB * nT.y + faceN * nT.z);

    // Underwater back face: match the surrounding fogged water volume instead
    if (!gl_FrontFacing) {
        vec3 viewUW = normalize(toCameraVector);
        vec3 reflLight = reflect(-sunDir, normal);
        float glint = pow(max(dot(reflLight, viewUW), 0.0), 80.0);
        float dayFactor = smoothstep(twilightLow, twilightHigh, sunDir.y);
        float dayT = smoothstep(-0.1, 0.2, sunDir.y);
        vec3 baseColor = mix(vec3(0.0, 0.096, 0.085), underwaterFogColor, dayT);
        vec3 col = baseColor + lightColor * glint * 2.0 * dayFactor;
        float d = length(toCameraVector);
        float a = mix(0.12, 0.85, smoothstep(2.0, 35.0, d));
        FragColor = vec4(col, a);
        return;
    }

    vec3 viewVector = normalize(toCameraVector);
    vec3 viewIncoming = -viewVector;

    // Sky reflection. Perturb in the face's tangent plane (T, B) so the
    // jitter is consistent across face orientations - the old `reflectDir.xz`
    // shake assumed a horizontal surface.
    vec3 reflectDir = reflect(viewIncoming, normal);
    reflectDir += (faceT * totalDistortion.x + faceB * totalDistortion.y);
    reflectDir = normalize(reflectDir);
    vec3 reflectColor = sampleSkyColor(skyLUT, reflectDir, sunDir, skyExposure);

    vec3 waterColor = vec3(0.0, 0.3, 0.5);
    vec3 refractColor = waterColor;

    // Fresnel
    float refractiveFactor = clamp(dot(viewVector, normal), 0.001, 0.999);

    // Soft + sharp specular stacked - same pattern as ocean shader.
    vec3 reflectedLight = reflect(-sunDir, normal);
    float specSoft  = pow(max(dot(reflectedLight, viewVector), 0.0), shineDamper);
    float specSharp = pow(max(dot(reflectedLight, viewVector), 0.0), 200.0);
    vec3 specularHighlights = lightColor * (specSoft * reflectivity + specSharp * 1.5);
    float dayFactor = smoothstep(twilightLow, twilightHigh, sunDir.y);
    specularHighlights *= dayFactor;

    vec3 col = mix(reflectColor, refractColor, refractiveFactor);
    col = mix(col, waterColor, 0.2) + specularHighlights;
    FragColor = vec4(col, 0.75);

    // Distance-based horizon mix - even small ponds benefit a little when
    // looking across a long stretch of placed water.
    float viewDist = length(toCameraVector);
    {
        vec3 viewDirToFrag = normalize(-toCameraVector);
        vec3 horizonColor = sampleSkyColor(skyLUT, viewDirToFrag, sunDir, skyExposure);
        float horizonMix = smoothstep(40.0, 250.0, viewDist) * 0.35;
        FragColor.rgb = mix(FragColor.rgb, horizonColor, horizonMix);
    }

    if (fogEnabled) {
        float fogFactor = 1.0 - pow(smoothstep(fogStart, fogEnd, viewDist), fogStrength);
        vec3 viewDir = normalize(-toCameraVector);
        vec3 fogColor = sampleSkyColor(skyLUT, viewDir, sunDir, skyExposure);
        FragColor.rgb = mix(fogColor, FragColor.rgb, fogFactor);
        FragColor.a *= fogFactor;
    }
}
