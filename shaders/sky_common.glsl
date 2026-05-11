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

// Uncharted2 filmic tone mapping. exposure is passed explicitly so both
// skyLUT_render.frag (uniform float exposure) and fog shaders
// (uniform float skyExposure) can use the same function.
vec3 skyUncharted2(vec3 color, float exposure) {
    const float A=0.15, B=0.50, C=0.10, D=0.20, E=0.02, F=0.30, W=11.2, gamma=2.2;
    color *= exposure;
    color = ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F)) - E/F;
    float white = ((W*(A*W+C*B)+D*E)/(W*(A*W+B)+D*F)) - E/F;
    color /= white;
    return pow(max(color, vec3(0.0)), vec3(1.0/gamma));
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
    return skyUncharted2(sampleSkyColorLinear(lut, viewDir, sunDir), exposure);
}
