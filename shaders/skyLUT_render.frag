#version 460 core

#define M_PI 3.1415926535897932384626433832795

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
uniform vec3 sunDir;

// Underwater rendering
uniform bool cameraUnderwater;
uniform vec3 underwaterFogColor;

// LUT
uniform sampler2D skyLUT;

// Low-res cloud composite
uniform int cloudsCompositeEnabled;
uniform sampler2D cloudTex;

// Constants (must match LUT generator)
const float innerRadius = 1.0;
const float outerRadius = 1.025;
const float G = 0.76;

// Phase functions
float rayleighPhase(float mu) {
    return (3.0 / (16.0 * M_PI)) * (1.0 + mu * mu);
}

float miePhase(float mu) {
    float g2 = G * G;
    float denom = pow(1.0 + g2 - 2.0 * G * mu, 1.5);
    return (3.0 / (8.0 * M_PI)) * (1.0 - g2) * (1.0 + mu * mu) / ((2.0 + g2) * denom);
}

vec3 Uncharted2ToneMapping(vec3 color) {
    float gamma = 2.2;
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;
    color *= exposure;
    color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;
    float white = ((W * (A * W + C * B) + D * E) / (W * (A * W + B) + D * F)) - E / F;
    color /= white;
    color = pow(color, vec3(1.0 / gamma));
    return color;
}

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
    vec3 col = rayleighScatter * rayleighPhase(mu)
             + vec3(mieScatter) * miePhase(mu);

    // Sun disk + soft halo
    float sunAng = acos(mu);
    float disk = smoothstep(0.010, 0.006, sunAng);
    float halo = exp(-sunAng * 40.0) * 0.4;
    vec3 sunCol = vec3(1.0, 0.98, 0.90) * 30.0;
    col += (disk + halo) * sunCol;

    // Cloud composite
    if (cloudsCompositeEnabled != 0)
    {
        vec2 uv = (gl_FragCoord.xy + vec2(0.5)) / max(resolution, vec2(1.0));
        vec4 cloud = texture(cloudTex, uv);

        const float cloudY = 145.0;
        float t = (cloudY - cameraPosWorld.y) / r.y;

        float cloudOpacity = 1.0 - cloud.a;

        if (t > 0.0 && cloudOpacity > 0.01) {
            vec3 cloudIntersection = cameraPosWorld + t * r;
            vec4 cloudClip = projection * view * vec4(cloudIntersection, 1.0);
            float cloudDepthNDC = cloudClip.z / cloudClip.w;
            float cloudDepth = clamp(cloudDepthNDC * 0.5 + 0.5, 0.0, 1.0);
            gl_FragDepth = mix(1.0, cloudDepth, cloudOpacity);
        } else {
            gl_FragDepth = 1.0;
        }

        col = cloud.rgb + cloud.a * col;
    } else {
        gl_FragDepth = 1.0;
    }

    // Tone mapping
    vec3 mapped = vec3(1.0) - exp(-exposure * col);
    vec3 tone = Uncharted2ToneMapping(col);

    // Apply underwater fog to sky
    if (cameraUnderwater) {
        // Replace sky with murky water fog color
        tone = underwaterFogColor;
    }

    FragColor = vec4(tone, 1.0);
}
