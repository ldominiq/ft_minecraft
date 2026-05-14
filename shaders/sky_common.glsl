#define M_PI 3.1415926535897932384626433832795

const float SKY_G = 0.76;

float skyRayleighPhase(float mu) {
    return (3.0 / (16.0 * M_PI)) * (1.0 + mu * mu);
}

float skyMiePhase(float mu) {
    float g2 = SKY_G * SKY_G;
    float denom = pow(1.0 + g2 - 2.0 * SKY_G * mu, 1.5);
    return (3.0 / (8.0 * M_PI)) * (1.0 - g2) * (1.0 + mu * mu) / ((2.0 + g2) * denom);
}

// ACES filmic tone mapping (Krzysztof Narkowicz's cheap fit, 2015).
// Exposure is passed explicitly so both skyLUT_render.frag (uniform float
// exposure) and the fog shaders (uniform float skyExposure) can call the same
// function. The final pow(1/2.2) is the manual sRGB-encode step
// (no GL_FRAMEBUFFER_SRGB in this pipeline).
vec3 skyTonemap(vec3 color, float exposure) {
    color *= exposure;
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    vec3 tm = clamp((color*(a*color+b)) / (color*(c*color+d)+e), 0.0, 1.0);
    return pow(max(tm, vec3(0.0)), vec3(1.0/2.2));
}

// Linear (un-tonemapped) variant: returns raw HDR radiance from the LUT after
// applying phase functions. Used when the caller will tonemap once later (HDR
// pipeline). The tonemapped variant below is kept for LDR callers.
vec3 sampleSkyColorLinear(sampler2D lut, vec3 viewDir, vec3 sunDir) {
    vec2 lutUV = vec2(viewDir.y * 0.5 + 0.5, sunDir.y * 0.5 + 0.5);
    vec4 scatter = texture(lut, lutUV);
    float mu = dot(viewDir, sunDir);
    return scatter.rgb * skyRayleighPhase(mu) + vec3(scatter.a) * skyMiePhase(mu);
}

// Sample the precomputed scattering LUT at (viewDir, sunDir),
// apply phase functions, and tone-map.
vec3 sampleSkyColor(sampler2D lut, vec3 viewDir, vec3 sunDir, float exposure) {
    return skyTonemap(sampleSkyColorLinear(lut, viewDir, sunDir), exposure);
}
