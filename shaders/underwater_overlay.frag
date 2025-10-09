#version 460 core
in vec2 vUV;
out vec4 FragColor;

uniform float uTime;
uniform vec2 uResolution;

// Scene inputs (from refraction FBO)
uniform sampler2D uSceneColor;   // refraction color texture
uniform sampler2D uSceneDepth;    // refraction depth texture

// Distortion inputs
uniform sampler2D uDuDv;          // scrolling dudv map
uniform float uMove;              // 0..1 scroll factor

// Camera params
uniform float uNear;
uniform float uFar;

// Fog/tint controls
uniform vec3 uFogColor;           // blue-green fog color
uniform float uFogDensity;        // base fog density
uniform vec3 uTintColor;          // subtle tint
uniform float uOpacity;           // overlay blend strength

// Helpers
float linearizeDepth(float d, float n, float f) {
    float z = d * 2.0 - 1.0;
    return (2.0 * n * f) / (f + n - z * (f - n));
}

float vignette(vec2 uv) {
    vec2 p = uv * 2.0 - 1.0; // to [-1,1]
    float r = length(p);
    float v = smoothstep(0.95, 0.2, r);
    return clamp(v, 0.0, 1.0);
}

void main() {
    // Two-octave dudv distortion in screen space
    // Scales chosen so patterns are large and slow, like underwater shimmer
    vec2 uv = vUV;
    vec2 duv1 = texture(uDuDv, vec2(uv.x * 0.6 + uMove, uv.y * 0.6)).rg * 2.0 - 1.0;
    vec2 duv2 = texture(uDuDv, vec2(uv.x * 0.25 - uMove * 0.5, uv.y * 0.25 + uMove * 0.35)).rg * 2.0 - 1.0;
    vec2 dudv = mix(duv1, duv2, 0.45);

    // Depth-aware distortion: shallower = stronger wiggle, fades with distance
    float sceneDepthNDC = texture(uSceneDepth, uv).r;
    float sceneDist = linearizeDepth(sceneDepthNDC, uNear, uFar);
    float depthFactor = clamp(sceneDist / 6.0, 0.0, 1.0);
    float waveStrength = mix(0.018, 0.006, depthFactor);
    vec2 offset = dudv * waveStrength;

    vec2 uvDistorted = clamp(uv + offset, vec2(0.001), vec2(0.999));

    vec3 sceneCol = texture(uSceneColor, uvDistorted).rgb;

    // Distance fog based on scene depth (exponential squared)
    float fogStrength = 1.0 - exp(-uFogDensity * uFogDensity * sceneDist * sceneDist);
    fogStrength = clamp(fogStrength, 0.0, 1.0);

    // Slight color absorption toward blue-green with depth
    vec3 fogged = mix(sceneCol, uFogColor, fogStrength);

    // Add a very soft tint and a faint caustic shimmer modulation
    float wave = sin((uv.y * uResolution.y * 0.01) + uTime * 2.0) * 0.03;
    float wave2 = sin((uv.y * uResolution.y * 0.021 + 1.2) - uTime * 1.4) * 0.02;
    float wobble = (wave + wave2) * 0.5; // -0.025..0.025

    float vig = vignette(uv);

    vec3 tinted = mix(fogged, fogged * uTintColor, 0.12);

    // Compose: blend the distorted, fogged scene over the already-rendered frame.
    // This creates a heat-haze-like refraction without requiring the actual backbuffer.
    float overlayAlpha = uOpacity * (0.92 + wobble * 0.5) * mix(0.8, 1.0, vig);

    FragColor = vec4(tinted, clamp(overlayAlpha, 0.0, 1.0));
}
