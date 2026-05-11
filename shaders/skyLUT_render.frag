#version 460 core

/*
 * Sky shader using precomputed scattering LUT.
 *
 * The LUT is parameterized by:
 *   U = view zenith cosine  (mapped from [-1,1] to [0,1])
 *   V = sun zenith cosine   (mapped from [-1,1] to [0,1])
 *
 * The LUT stores:
 *   rgb = Rayleigh in-scatter (with scattering coefficients * ESun pre-multiplied)
 *   a   = Mie in-scatter (scalar, with scattering coefficient * ESun pre-multiplied)
 *
 * At lookup time we only need to apply the phase functions (which depend
 * on the angle between the view ray and the sun direction) and tone-map.
 */

out vec4 FragColor;

uniform vec2 resolution;
uniform mat4 view;
uniform mat4 projection;
uniform vec3 cameraPosWorld;
uniform float exposure;
// When false, output linear HDR radiance (scene FBO is RGBA16F and the final
// tonemap is done once in clouds_composite). Defaults to true for the LDR path.
uniform bool tonemapHere;
uniform vec3 sunDir;

// Underwater rendering
uniform bool cameraUnderwater;
uniform vec3 underwaterFogColor;

// LUT
uniform sampler2D skyLUT;

// Low-res cloud composite

#include "sky_common.glsl"

// Constants (must match LUT generator)
const float innerRadius = 1.0;
const float outerRadius = 1.025;

void main() {
    // Reconstruct world-space ray direction
    vec2 ndc = (gl_FragCoord.xy / max(resolution, vec2(1.0))) * 2.0 - 1.0;
    vec4 clip = vec4(ndc, -1.0, 1.0);
    vec4 viewPos = inverse(projection) * clip;
    vec3 rayView = normalize(viewPos.xyz / max(abs(viewPos.w), 1e-6));
    vec3 r = normalize((inverse(view) * vec4(rayView, 0.0)).xyz);

    // View zenith cosine (r.y = cos angle from up in world space)
    // We use the world up direction as zenith
    float viewZenithCos = r.y;

    // Sun zenith cosine
    float sunZenithCos = sunDir.y;

    // LUT UV mapping: cosine in [-1,1] -> UV in [0,1]
    vec2 lutUV = vec2(
        viewZenithCos * 0.5 + 0.5,
        sunZenithCos * 0.5 + 0.5
    );

    // Sample the precomputed scattering
    vec4 scatter = texture(skyLUT, lutUV);
    vec3 rayleighScatter = scatter.rgb;
    float mieScatter = scatter.a;

    // Apply phase functions (view-sun angle dependent)
    float mu = clamp(dot(r, sunDir), -1.0, 1.0);
    vec3 col = rayleighScatter * skyRayleighPhase(mu)
             + vec3(scatter.a) * skyMiePhase(mu);

    // Sun disk + soft halo
    float sunAng = acos(mu);
    float disk = smoothstep(0.010, 0.006, sunAng);
    float halo = exp(-sunAng * 40.0) * 0.4;
    vec3 sunCol = vec3(1.0, 0.98, 0.90) * 30.0;
    col += (disk + halo) * sunCol;

    gl_FragDepth = 1.0;

    // In LDR mode: tone-map + gamma here. In HDR mode: emit linear radiance and
    // let the final composite tonemap once.
    vec3 tone = tonemapHere ? skyUncharted2(col, exposure) : col;

    // Apply underwater fog to sky. In HDR mode lift the sRGB-display color to
    // linear so it survives the final pow(1/2.2) without darkening.
    if (cameraUnderwater) {
        tone = tonemapHere ? underwaterFogColor : pow(underwaterFogColor, vec3(2.2));
    }

    FragColor = vec4(tone, 1.0);
}
