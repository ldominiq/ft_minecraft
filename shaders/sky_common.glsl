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

// Sample the precomputed scattering LUT at (viewDir, sunDir),
// apply phase functions, and tone-map.
vec3 sampleSkyColor(sampler2D lut, vec3 viewDir, vec3 sunDir, float exposure) {
    vec2 lutUV = vec2(viewDir.y * 0.5 + 0.5, sunDir.y * 0.5 + 0.5);
    vec4 scatter = texture(lut, lutUV);
    float mu = dot(viewDir, sunDir);
    vec3 color = scatter.rgb * skyRayleighPhase(mu) + vec3(scatter.a) * skyMiePhase(mu);
    return skyUncharted2(color, exposure);
}

// Henyey-Greenstein with tunable g. Used for ground-fog in-scatter and
// volumetric god-rays. Returns a normalised phase value (integral over the
// sphere = 1).
float miePhaseHG(float mu, float g) {
    float g2 = g * g;
    float denom = pow(max(1.0 + g2 - 2.0 * g * mu, 1e-4), 1.5);
    return (1.0 - g2) / (4.0 * M_PI * denom);
}

// Distance fog with optional sun-direction Mie in-scatter. When mieStrength
// is 0 this collapses to the previous mix(skyColor, sceneColor, fogFactor).
// fogDir / sunDir must be unit-length and point *toward* the fragment / sun
// respectively (matches sampleSkyColor convention).
vec3 applyDistanceFog(vec3 sceneColor, vec3 fogDir, vec3 sunDir,
                      sampler2D skyLUT, float skyExposure,
                      float fogStart, float fogEnd, float fogStrength,
                      float dist, float mieG, float mieStrength) {
    float fogFactor = 1.0 - pow(smoothstep(fogStart, fogEnd, dist), fogStrength);
    vec3 fogCol = sampleSkyColor(skyLUT, fogDir, sunDir, skyExposure);
    if (mieStrength > 0.0) {
        float mu = dot(fogDir, sunDir);
        // Bias toward sun-facing in-scatter only. The dot is clamped so the
        // anti-sun half of the sky stays untouched (otherwise heavy-g HG
        // produces a small uniform DC term that washes out the horizon).
        float forward = max(mu, 0.0);
        float mie = miePhaseHG(forward, mieG);
        fogCol += fogCol * mie * mieStrength;
    }
    return mix(fogCol, sceneColor, fogFactor);
}
