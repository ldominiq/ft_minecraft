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
float cloudDensityAt(vec3 pWorld)
{
    float h = (pWorld.y - cloudBoxMinWorld.y) / max(cloudBoxMaxWorld.y - cloudBoxMinWorld.y, 1e-3);
    h = clamp(h, 0.0, 1.0);

    // Soft bottom/top
    float profile = smoothstep(0.0, 0.15, h) * (1.0 - smoothstep(0.80, 1.0, h));
    return cloudDensity * profile;
}

vec4 marchCloudCube(vec3 roWorld, vec3 rdWorld)
{
    float t0, t1;
    if (!intersectAABB(roWorld, rdWorld, cloudBoxMinWorld, cloudBoxMaxWorld, t0, t1))
        return vec4(0.0, 0.0, 0.0, 1.0); // no cloud, T=1

    float len = max(t1 - t0, 0.0);

    // Adaptive steps: avoid wasting work on small intersections
    float steps = clamp(len / 3.0, 8.0, max(8.0, cloudStepCount));
    float dt = len / steps;

    vec3  col = vec3(0.0);
    float T   = 1.0;

    float mu = dot(rdWorld, normalize(sunDir));
    float phase = hgPhase(clamp(mu, -1.0, 1.0), clamp(cloudPhaseG, -0.99, 0.99));

    vec3 sunLight = cloudSunColor * cloudSunStrength;

    for (int i = 0; i < 128; ++i)
    {
        if (float(i) >= steps) break;

        float t = t0 + (float(i) + 0.5) * dt;
        vec3  p = roWorld + rdWorld * t;

        float d = max(cloudDensityAt(p), 0.0);

        float sigma_t = cloudSigmaT * d;
        float sigma_s = cloudSigmaS * d;

        float Tr = exp(-sigma_t * dt);
        float absorbed = 1.0 - Tr;

        // Direct (no self-shadowing yet, kept fast)
        vec3 direct = cloudAlbedo * sunLight * (sigma_s * phase);

        // Ambient fill
        vec3 ambient = cloudAlbedo * cloudAmbientColor * (sigma_s * cloudAmbientStrength);

        // Depth-based ambient boost (cheap multi-scatter approximation)
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