#version 460 core

/*
 * Sky Scattering Look-Up Table Generator
 *
 * Precomputes atmospheric scattering into a 2D texture:
 *   U = view zenith angle   (0 = horizon, 1 = straight up)
 *   V = sun zenith angle    (0 = horizon, 1 = sun at zenith)
 *
 * For each (U,V) pair we integrate Rayleigh + Mie scattering along
 * a vertical ray at the given view elevation, with the sun at the
 * given elevation. The result stores the in-scattered light *before*
 * the phase function is applied, split into Rayleigh and Mie channels
 * so the final sky shader can apply the view-dependent phase at lookup time.
 *
 * Output: vec4(sumR.rgb, sumM_luminance)
 *   rgb  = Rayleigh in-scatter (wavelength-dependent)
 *   a    = Mie in-scatter (grayscale, wavelength-independent)
 */

out vec4 FragColor;

uniform vec2 lutSize;        // width, height of the LUT texture
uniform float atmDensity;
uniform float atmThickness;
uniform float cameraPosY;    // camera world Y position
uniform float seaLevel;      // world sea level
uniform float planetScale;   // world-to-planet scale factor

// --- Constants (must match sky.frag) ---
const float innerRadius = 1.0;
const float outerRadius = 1.025;
const float HR = 0.25;
const float HM = 0.10;
const float Kr = 0.0025;
const float Km = 0.0010;
const float ESun = 40.0;

const vec3 invWavelength4 = vec3(
    pow(0.650, -4.0),
    pow(0.570, -4.0),
    pow(0.475, -4.0)
);

bool intersectSphere(vec3 ro, vec3 rd, float radius, out float t0, out float t1) {
    float a = dot(rd, rd);
    float b = 2.0 * dot(ro, rd);
    float c = dot(ro, ro) - radius * radius;
    float disc = b*b - 4.0*a*c;
    if (disc < 0.0) { t0 = t1 = -1.0; return false; }
    float s = sqrt(disc);
    float inv2a = 0.5 / a;
    t0 = (-b - s) * inv2a;
    t1 = (-b + s) * inv2a;
    if (t0 > t1) { float tmp = t0; t0 = t1; t1 = tmp; }
    return true;
}

void main() {
    // UV in [0,1]
    vec2 uv = gl_FragCoord.xy / lutSize;

    // Decode angles from UV
    // U -> view direction zenith cosine: -1 (down) to +1 (up)
    //   We use a non-linear mapping to give more precision near the horizon
    float viewZenithCos = uv.x * 2.0 - 1.0;

    // V -> sun zenith cosine: -1 (below horizon) to +1 (overhead)
    float sunZenithCos = uv.y * 2.0 - 1.0;

    // Build the view ray and sun direction in a 2D vertical plane
    // Eye position accounts for player height in planet space
    float heightWorld = max(cameraPosY - seaLevel, 0.0);
    float heightPlanet = heightWorld / planetScale;

    // Clamp so the eye never exits the atmosphere shell
    float maxAlt = (outerRadius - innerRadius) - 1e-4;
    heightPlanet = min(heightPlanet, maxAlt);

    vec3 eye = vec3(0.0, innerRadius + heightPlanet, 0.0);

    // View direction from zenith cosine
    float viewZenithSin = sqrt(max(1.0 - viewZenithCos * viewZenithCos, 0.0));
    vec3 dir = vec3(viewZenithSin, viewZenithCos, 0.0);

    // Sun direction from zenith cosine (in the same plane)
    float sunZenithSin = sqrt(max(1.0 - sunZenithCos * sunZenithCos, 0.0));
    vec3 sunDir = vec3(sunZenithSin, sunZenithCos, 0.0);

    // Intersect view ray with atmosphere
    float t0o, t1o;
    if (!intersectSphere(eye, dir, outerRadius, t0o, t1o) || t1o <= 0.0) {
        FragColor = vec4(0.0);
        return;
    }
    float farDist = t1o;

    // Integration
    const int SAMPLES = 16;     // More samples than runtime — this is precomputed
    const int SAMPLES_SUN = 8;
    float segment = farDist / float(SAMPLES);

    float optR = 0.0;
    float optM = 0.0;
    vec3 sumR = vec3(0.0);
    float sumM = 0.0;

    float HR_eff = max(1e-4, HR * atmThickness);
    float HM_eff = max(1e-4, HM * atmThickness);

    for (int i = 0; i < SAMPLES; ++i) {
        float t = (float(i) + 0.5) * segment;
        vec3 pos = eye + dir * t;
        float height = length(pos);
        float alt = max(height - innerRadius, 0.0);
        float localR = atmDensity * exp(-alt / HR_eff);
        float localM = atmDensity * exp(-alt / HM_eff);
        optR += localR * segment;
        optM += localM * segment;

        // Sun ray optical depth
        float ts0, ts1;
        float odRsun = 0.0;
        float odMsun = 0.0;
        if (intersectSphere(pos, sunDir, outerRadius, ts0, ts1)) {
            float sunLen = max(ts1, 0.0);
            float segSun = sunLen / float(SAMPLES_SUN);
            vec3 sPos = pos;
            for (int j = 0; j < SAMPLES_SUN; ++j) {
                sPos += sunDir * segSun;
                float hSun = length(sPos);
                float aSun = max(hSun - innerRadius, 0.0);
                odRsun += (atmDensity * exp(-aSun / HR_eff)) * segSun;
                odMsun += (atmDensity * exp(-aSun / HM_eff)) * segSun;
            }
        }

        vec3 tauR = (Kr * atmDensity) * invWavelength4 * (optR + odRsun);
        float tauM = (Km * atmDensity) * (optM + odMsun);
        vec3 atten = exp(-(tauR + vec3(tauM)));

        // Accumulate Rayleigh (wavelength-dependent) and Mie (scalar) separately
        sumR += localR * atten * segment;
        sumM += localM * dot(atten, vec3(1.0/3.0)) * segment;  // average attenuation for Mie
    }

    // Pre-multiply by scattering coefficients and sun intensity
    // Phase functions will be applied at lookup time (they depend on view-sun angle)
    vec3 rayleighResult = sumR * ((Kr * atmDensity) * invWavelength4) * ESun;
    float mieResult = sumM * ((Km * atmDensity)) * ESun;

    FragColor = vec4(rayleighResult, mieResult);
}
