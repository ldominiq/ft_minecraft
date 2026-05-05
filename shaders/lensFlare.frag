#version 460 core
//
// Procedural sprite-based lens flare. Generates 8 ghost / halo splats
// along the line (sunUV -> screenCenter), modulates by an occlusion test
// against scene depth, outputs as an additive RGB.
//
// The fragment shader runs over the entire screen; each pixel evaluates
// its contribution from every ghost. Costs a handful of texture taps + a
// small constant ALU loop — fine for 1080p.
//
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D sceneDepth;
uniform vec2  sunUV;       // sun screen position in [0,1]^2
uniform vec2  screenSize;
uniform float intensity;

// Aspect-corrected distance — keeps splats round on non-square screens.
float aspectDist(vec2 a, vec2 b) {
    vec2 d = (a - b) * vec2(screenSize.x / screenSize.y, 1.0);
    return length(d);
}

float gauss(float r, float sigma) {
    return exp(-(r * r) / (2.0 * sigma * sigma));
}

// 9-tap visibility — sun is "visible" at a pixel iff that pixel sampled
// the sky (depth == 1). A small disk hides single-pixel acne and gives a
// smooth fade across leaves and block edges.
float sunVisibility(vec2 uv) {
    if (any(lessThan(uv, vec2(0.0))) || any(greaterThan(uv, vec2(1.0))))
        return 0.0;

    const int   N = 9;
    const float radiusPx = 8.0;
    vec2 texel = 1.0 / screenSize;

    float lit = 0.0;
    for (int i = 0; i < N; ++i) {
        // Cheap deterministic disk samples.
        float a = 6.2831853 * float(i) / float(N);
        vec2 off = vec2(cos(a), sin(a)) * radiusPx * texel;
        // Use random radius too so we don't sample only on a ring.
        off *= mix(0.3, 1.0, fract(sin(float(i) * 12.9898) * 43758.5453));
        float d = texture(sceneDepth, uv + off).r;
        lit += step(0.99999, d);
    }
    return lit / float(N);
}

// One ghost / splat. center ∈ [0,1]^2.
vec3 splat(vec2 uv, vec2 center, float sigma, vec3 tint, float scale) {
    float r = aspectDist(uv, center);
    return tint * gauss(r, sigma) * scale;
}

void main() {
    vec3 result = vec3(0.0);

    float vis = sunVisibility(sunUV);
    if (vis <= 0.0) {
        FragColor = vec4(0.0);
        return;
    }

    // Fade as the sun approaches / leaves the screen edge so flare doesn't
    // pop on/off when it crosses the frustum boundary.
    float edgeFade = smoothstep(1.25, 0.85, length(sunUV * 2.0 - 1.0));

    // Direction from sun toward the screen center; ghosts are placed at
    // parametric offsets along this axis.
    vec2 toCenter = vec2(0.5) - sunUV;

    // 1) The bright halo around the sun itself.
    result += splat(TexCoords, sunUV, 0.10, vec3(1.0, 0.95, 0.85), 1.6);
    result += splat(TexCoords, sunUV, 0.03, vec3(1.0, 1.0, 1.0),   3.5);

    // 2) Anamorphic horizontal streak through the sun.
    {
        vec2 d = (TexCoords - sunUV) * vec2(1.0, 12.0);
        float r = length(d);
        result += vec3(0.6, 0.7, 1.0) * gauss(r, 0.15) * 0.6;
    }

    // 3) Ghost chain along sunUV -> center (and slightly past).
    //    Each entry: (t along axis, sigma, tint, scale)
    const int kGhosts = 7;
    vec4 ghostT[7] = vec4[7](
        vec4(0.30, 0.05, 1.0, 0.60),
        vec4(0.50, 0.08, 1.0, 0.50),
        vec4(0.70, 0.04, 1.0, 0.45),
        vec4(0.95, 0.06, 1.0, 0.55),
        vec4(1.10, 0.10, 1.0, 0.40),
        vec4(1.35, 0.05, 1.0, 0.35),
        vec4(1.60, 0.07, 1.0, 0.30)
    );
    vec3 tints[7] = vec3[7](
        vec3(1.00, 0.55, 0.40),
        vec3(0.80, 0.85, 1.00),
        vec3(0.60, 1.00, 0.70),
        vec3(1.00, 0.85, 0.55),
        vec3(0.55, 0.65, 1.00),
        vec3(1.00, 0.60, 0.60),
        vec3(0.85, 0.85, 1.00)
    );
    for (int i = 0; i < kGhosts; ++i) {
        vec2 c = sunUV + toCenter * ghostT[i].x;
        result += splat(TexCoords, c, ghostT[i].y, tints[i], ghostT[i].w);
    }

    FragColor = vec4(result * vis * edgeFade * intensity, 1.0);
}
