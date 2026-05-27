#version 460 core

#define M_PI 3.1415926535897932384626433832795

/*
Accurate Atmospherical Scattering from GPUgems 2
The two most common forms of scattering in the atmosphere are Rayleigh scattering and Mie scattering.

Rayleigh scattering is caused by small molecules in the air, and it scatters light more heavily at the shorter wavelengths
(blue first, then green, and then red).

Mie scattering is caused by larger particles in the air called aerosols (such as dust and pollution),
and it tends to scatter all wavelengths of light equally. On a hazy day, Mie scattering causes the sky to look a bit gray and
causes the sun to have a large white halo around it. Mie scattering can also be used to simulate light scattered from small
particles of water and ice in the air, to produce effects like rainbows
*/

out vec4 FragColor;

// Uniforms for OpenGL
uniform vec2 resolution;
uniform float time;
uniform mat4 view;       // camera view
uniform mat4 projection; // camera projection
uniform vec3 cameraPosWorld;
uniform float seaLevel;
uniform float exposure;  // exposure for simple tone mapping (1 - exp(-exposure * color))
// When false, the scene framebuffer is HDR (RGBA16F) and the final tonemap is
// performed once in clouds_composite. We must NOT tonemap here in that case -
// emit linear radiance instead. Defaults to true for the LDR path.
uniform bool tonemapHere;
uniform float atmDensity;    // 1.0 = Earth-like, lower -> closer to space
uniform float atmThickness;  // scales HR/HM (1.0 = Earth-like)
uniform float planetScale;
uniform vec3 sunDir;

// Underwater rendering
uniform bool cameraUnderwater;
uniform vec3 underwaterFogColor;


// -----------------------------
// Constants (O'Neil/GPU Gems 2)
// -----------------------------


// Normalized planet radii (center at 0)
const float innerRadius = 1.0;      // ground/surface radius
const float outerRadius = 1.025;    // top of atmosphere

// Scale heights (relative to innerRadius)
const float HR = 0.25;              // Rayleigh scale height
const float HM = 0.10;              // Mie scale height

// Scattering constants
const float Kr = 0.0025;            // Rayleigh scattering constant
const float Km = 0.0010;            // Mie scattering constant
const float ESun = 40.0;            // Sun intensity
const float G = 0.76;               // Mie phase asymmetry

// 1 / wavelength^4 (approx RGB wavelengths in micrometers)
const vec3 invWavelength4 = vec3(
    pow(0.650, -4.0),
    pow(0.570, -4.0),
    pow(0.475, -4.0)
);

// -----------------------------
// Helpers
// -----------------------------

bool intersectSphere(vec3 ro, vec3 rd, float radius, out float t0, out float t1) {
    // Solve |ro + rd*t|^2 = r^2
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

float rayleighPhase(float mu) {
    return (3.0 / (16.0 * M_PI)) * (1.0 + mu * mu);
}

float miePhase(float mu) {
    float g2 = G * G;
    float denom = pow(1.0 + g2 - 2.0 * G * mu, 1.5);
    return (3.0 / (8.0 * M_PI)) * (1.0 - g2) * (1.0 + mu * mu) / ((2.0 + g2) * denom);
}

// Integrate scattering along the eye ray using O'Neil's approach
vec3 getSkyColor(vec3 eye, vec3 dir, vec3 sunDir) {
    // Intersect with atmosphere
    float t0o, t1o; // outer sphere intersections
    if (!intersectSphere(eye, dir, outerRadius, t0o, t1o)) {
        // Ray misses the atmosphere; return space blue
        return vec3(0.25, 0.35, 0.6);
    }
    float farDist = t1o; // far intersection with outer sphere
    if (farDist <= 0.0) {
        // We're inside and looking away; give a faint blue
        return vec3(0.25, 0.35, 0.6) * 0.2;
    }

    const int SAMPLES = 6;
    const int SAMPLES_SUN = 3;
    float segment = farDist / float(SAMPLES);

    float optR = 0.0; // optical depth along view (Rayleigh)
    float optM = 0.0; // optical depth along view (Mie)

    vec3 sumR = vec3(0.0);
    vec3 sumM = vec3(0.0);

    // Effective scale heights
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

        // Sun ray optical depth from this sample to atmosphere edge
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

        sumR += localR * atten * segment;
        sumM += localM * atten * segment;
    }

    float mu = clamp(dot(dir, sunDir), -1.0, 1.0);
    vec3 col = sumR * ((Kr * atmDensity) * invWavelength4) * ESun * rayleighPhase(mu)
             + sumM * ((Km * atmDensity)) * ESun * miePhase(mu);

    return col;
}

void main() {
    // NDC from screen
    vec2 ndc = (gl_FragCoord.xy / max(resolution, vec2(1.0))) * 2.0 - 1.0;

    // Reconstruct view-space ray then transform to world-space
    vec4 clip = vec4(ndc, -1.0, 1.0);
    vec4 viewPos = inverse(projection) * clip;
    vec3 rayView = normalize(viewPos.xyz / max(abs(viewPos.w), 1e-6));
    vec3 r = normalize((inverse(view) * vec4(rayView, 0.0)).xyz);

    // Sun direction in world space (vertical plane motion)
    //float timeScale = 0.2;
    //float t = time * timeScale;
    //vec3 sunDir = normalize(vec3(sin(t), cos(t), 0.0));

    // Eye is just above the ground in planet space (decoupled from world translation)
    //vec3 eye = vec3(0.0, innerRadius + 0.001, 0.0);

    float heightWorld = max(cameraPosWorld.y - seaLevel, 0.0);
    float heightPlanet = heightWorld / planetScale;

    // Clamp so the eye never exits the atmosphere shell
    float maxAlt = (outerRadius - innerRadius) - 1e-4;
    heightPlanet = min(heightPlanet, maxAlt);

    vec3 eye = vec3(0.0, innerRadius + heightPlanet, 0.0);

    vec3 col = getSkyColor(eye, r, sunDir);

    // Sun disk + soft halo for visibility
    float sunCos = clamp(dot(r, sunDir), -1.0, 1.0);
    float sunAng = acos(sunCos);
    float disk = smoothstep(0.010, 0.006, sunAng);
    float halo = exp(-sunAng * 40.0) * 0.4;
    vec3 sunCol = vec3(1.0, 0.98, 0.90) * 30.0;
    col += (disk + halo) * sunCol;

    gl_FragDepth = 1.0;

    // In LDR mode: simple exposure tonemap here. In HDR mode: emit linear radiance
    // and let the final composite (clouds_composite.frag) tonemap once.
    vec3 mapped = tonemapHere ? (vec3(1.0) - exp(-exposure * col)) : col;

    // Apply underwater fog to sky. The flat fog color is authored as an sRGB-display
    // value, so in HDR mode lift it to linear to compensate for the final pow(1/2.2).
    if (cameraUnderwater) {
        mapped = tonemapHere ? underwaterFogColor : pow(underwaterFogColor, vec3(2.2));
    }

    FragColor = vec4(mapped, 1.0);
}

