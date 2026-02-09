#version 460 core

#define M_PI 3.1415926535897932384626433832795

in vec2 vUV;
out vec4 FragColor;

// Same uniforms you set in Lighting::renderCloudsLowRes
uniform vec2  resolution;
uniform float time;
uniform mat4  view;
uniform mat4  projection;
uniform vec3  cameraPosWorld;
uniform vec3  sunDir;

uniform vec3  cloudBoxMinWorld;
uniform vec3  cloudBoxMaxWorld;

uniform float cloudDensity;
uniform float cloudSigmaT;
uniform vec3  cloudAlbedo;
uniform float cloudStepCount;

uniform float cloudSigmaS;
uniform float cloudPhaseG;

uniform vec3  cloudAmbientColor;
uniform float cloudAmbientStrength;

uniform vec3  cloudSunColor;
uniform float cloudSunStrength;

uniform float cloudEdgeFeather;
uniform float cloudNoiseScale;
uniform float cloudNoiseContrastLo;
uniform float cloudNoiseContrastHi;
uniform float cloudWindSpeed;
uniform vec2  cloudWindDir;

// -----------------------------
// Helpers
// -----------------------------
bool intersectAABB(vec3 ro, vec3 rd, vec3 bmin, vec3 bmax, out float tEnter, out float tExit)
{
    vec3 invD = 1.0 / max(abs(rd), vec3(1e-8)) * sign(rd);
    vec3 t0s = (bmin - ro) * invD;
    vec3 t1s = (bmax - ro) * invD;
    vec3 tsmaller = min(t0s, t1s);
    vec3 tbigger  = max(t0s, t1s);
    tEnter = max(max(tsmaller.x, tsmaller.y), tsmaller.z);
    tExit  = min(min(tbigger.x,  tbigger.y),  tbigger.z);
    return tExit >= max(tEnter, 0.0);
}

float hgPhase(float mu, float g)
{
    float g2 = g * g;
    float denom = pow(max(1.0 + g2 - 2.0 * g * mu, 1e-4), 1.5);
    return (1.0 - g2) / (4.0 * M_PI * denom);
}

// -----------------------------
// Cheap noise (optional later). For now: simple vertical profile only.
// -----------------------------

float hash12(vec2 p)
{
    // deterministic [0,1)
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

float hash13(vec3 p)
{
    p = fract(p * 0.1031);
    p += dot(p, p.yzx + 33.33);
    return fract((p.x + p.y) * p.z);
}

float valueNoise3D(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);
    vec3 u = f * f * (3.0 - 2.0 * f);

    float n000 = hash13(i + vec3(0,0,0));
    float n100 = hash13(i + vec3(1,0,0));
    float n010 = hash13(i + vec3(0,1,0));
    float n110 = hash13(i + vec3(1,1,0));
    float n001 = hash13(i + vec3(0,0,1));
    float n101 = hash13(i + vec3(1,0,1));
    float n011 = hash13(i + vec3(0,1,1));
    float n111 = hash13(i + vec3(1,1,1));

    float nx00 = mix(n000, n100, u.x);
    float nx10 = mix(n010, n110, u.x);
    float nx01 = mix(n001, n101, u.x);
    float nx11 = mix(n011, n111, u.x);

    float nxy0 = mix(nx00, nx10, u.y);
    float nxy1 = mix(nx01, nx11, u.y);

    return mix(nxy0, nxy1, u.z);
}

float fbm3(vec3 p)
{
    float sum = 0.0;
    float amp = 0.5;
    float freq = 1.0;
    for (int o = 0; o < 3; ++o)
    {
        sum += amp * valueNoise3D(p * freq);
        freq *= 2.0;
        amp *= 0.5;
    }
    return sum; // ~[0,1]
}

float cloudDensityAt(vec3 pWorld)
{
    // vertical
    float h = (pWorld.y - cloudBoxMinWorld.y) / max(cloudBoxMaxWorld.y - cloudBoxMinWorld.y, 1e-3);
    h = clamp(h, 0.0, 1.0);
    float vertical = smoothstep(0.0, 0.15, h) * (1.0 - smoothstep(0.80, 1.0, h));

    // edge feather
    vec3 dMin = pWorld - cloudBoxMinWorld;
    vec3 dMax = cloudBoxMaxWorld - pWorld;
    vec3 d = min(dMin, dMax);

    float feather = max(cloudEdgeFeather, 0.001);
    vec3 m = smoothstep(vec3(0.0), vec3(feather), d);
    float edgeMask = m.x * m.y * m.z;

    // noise + wind
    vec2 wdir = normalize(cloudWindDir);
    vec3 p = pWorld;
    p.xz += wdir * (time * cloudWindSpeed);

    float n = fbm3(p * cloudNoiseScale);
    n = smoothstep(cloudNoiseContrastLo, cloudNoiseContrastHi, n);

    return cloudDensity * vertical * edgeMask * n;
}

vec4 marchCloudCube(vec3 roWorld, vec3 rdWorld)
{
    float t0, t1;
    if (!intersectAABB(roWorld, rdWorld, cloudBoxMinWorld, cloudBoxMaxWorld, t0, t1))
        return vec4(0.0, 0.0, 0.0, 1.0);

    float len = max(t1 - t0, 0.0);
    float steps = clamp(len / 3.0, 8.0, max(8.0, cloudStepCount));
    float dt = len / steps;

    vec3  col = vec3(0.0);
    float T   = 1.0;

    float mu = dot(-rdWorld, normalize(sunDir));
    float phase = hgPhase(clamp(mu, -1.0, 1.0), clamp(cloudPhaseG, -0.99, 0.99));
    vec3 sunLight = cloudSunColor * cloudSunStrength;

    // Jitter: shift sampling by a fraction of dt per pixel (reduces banding)
    float j = hash12(gl_FragCoord.xy + time * 37.0);
    float tBase = t0 + j * dt;

    for (int i = 0; i < 128; ++i)
    {
        if (float(i) >= steps) break;

        float t = tBase + (float(i) + 0.5) * dt;
        vec3  p = roWorld + rdWorld * t;

        float d = max(cloudDensityAt(p), 0.0);

        float sigma_t = cloudSigmaT * d;
        float sigma_s = cloudSigmaS * d;

        float Tr = exp(-sigma_t * dt);
        float absorbed = 1.0 - Tr;

        vec3 direct  = cloudAlbedo * sunLight * (sigma_s * phase);
        vec3 ambient = cloudAlbedo * cloudAmbientColor * (sigma_s * cloudAmbientStrength);

        float depthBoost = mix(1.0, 3.0, clamp(1.0 - T, 0.0, 1.0));

        col += (T * absorbed) * (direct + ambient * depthBoost);

        T *= Tr;
        if (T < 0.01) break;
    }

    return vec4(col, T);
}

void main()
{
    // Reconstruct NDC from pixel coords (low-res buffer)
    vec2 ndc = (gl_FragCoord.xy / max(resolution, vec2(1.0))) * 2.0 - 1.0;

    // Reconstruct view ray then transform to world
    vec4 clip = vec4(ndc, -1.0, 1.0);
    vec4 viewPos = inverse(projection) * clip;
    vec3 rayView = normalize(viewPos.xyz / max(abs(viewPos.w), 1e-6));
    vec3 rdWorld = normalize((inverse(view) * vec4(rayView, 0.0)).xyz);

    vec3 roWorld = cameraPosWorld;

    vec4 cloud = marchCloudCube(roWorld, rdWorld);

    // Output: rgb = in-scattered light, a = transmittance to background
    FragColor = cloud;
}