#version 460 core

// Post-terrain cloud composite + (in HDR mode) final tonemap.
//
// Reads the resolved (non-MSAA) scene color/depth and the low-res cloud RGBA texture,
// and composites clouds over the scene per-pixel using actual scene depth — no
// gl_FragDepth hacks.
//
// Two paths:
//   - LDR (hdrMode=false): scene texture is RGBA8 already tonemapped per-shader,
//     clouds are HDR (in-scattered light). We tonemap the cloud RGB to LDR and
//     alpha-blend over the scene. Same behavior as before the HDR refactor.
//   - HDR (hdrMode=true): scene texture is RGBA16F linear radiance, clouds are
//     HDR. We blend in linear HDR space, then apply a single ACES tonemap +
//     gamma at the end.

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D sceneColor;
uniform sampler2D sceneDepth;
uniform sampler2D cloudTex;       // rgb = scattered light, a = transmittance (1 = clear)

uniform mat4 view;
uniform mat4 projection;
uniform mat4 invViewProj;         // inverse(projection * view)
uniform vec3 cameraPosWorld;
uniform vec2 resolution;          // unused for now; kept for future bilateral upsample

uniform float cloudLayerMinY;
uniform float cloudLayerMaxY;
uniform float exposure;           // matches Lighting::skyExposure used by sky shaders
uniform bool  hdrMode;            // true = blend in HDR + final tonemap here
// Post-tonemap saturation (1.0 = identity, >1 = punchier, <1 = washed).
// Block textures are sRGB-encoded but treated as linear, so the tonemap curve
// compresses midtone saturation.
uniform float saturation;

// ACES filmic tone mapping — Krzysztof Narkowicz's cheap fit (2015).
// Returns linear values; the explicit pow(1/2.2) below encodes to display space
// (we don't use GL_FRAMEBUFFER_SRGB, so gamma is done by hand).
vec3 tonemap(vec3 color, float exp_) {
    color *= exp_;
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    vec3 tm = clamp((color*(a*color+b)) / (color*(c*color+d)+e), 0.0, 1.0);
    return pow(max(tm, vec3(0.0)), vec3(1.0/2.2));
}

// Luminance-preserving saturation in display (post-gamma) space. s=1 is identity.
vec3 applySaturation(vec3 c, float s) {
    float lum = dot(c, vec3(0.2126, 0.7152, 0.0722));
    return max(mix(vec3(lum), c, s), vec3(0.0));
}

void main() {
    vec3 sceneCol = texture(sceneColor, vUV).rgb;   // LDR in legacy path, HDR in hdrMode
    float sceneZ  = texture(sceneDepth, vUV).r;     // [0,1] depth-buffer value
    vec4 cloud    = texture(cloudTex, vUV);         // .rgb = HDR in-scattered light, .a = transmittance
    float cloudOpacity = 1.0 - cloud.a;

    // Cloud-free pixel: pass scene through (tonemap if HDR).
    if (cloudOpacity <= 0.001) {
        vec3 c = hdrMode ? tonemap(sceneCol, exposure) : sceneCol;
        FragColor = vec4(applySaturation(c, saturation), 1.0);
        return;
    }

    // Reconstruct world-space view ray for this pixel.
    vec2 ndcXY = vUV * 2.0 - 1.0;
    vec4 farClip = invViewProj * vec4(ndcXY, 1.0, 1.0);
    vec3 farWorld = farClip.xyz / farClip.w;
    vec3 r = normalize(farWorld - cameraPosWorld);

    // Determine whether the cloud at this pixel is in front of the scene.
    bool composite = true;
    bool cameraInsideLayer = (cameraPosWorld.y >= cloudLayerMinY && cameraPosWorld.y <= cloudLayerMaxY);

    if (!cameraInsideLayer) {
        // Outside the layer: intersect the ray with the appropriate near face,
        // project to NDC depth, and compare against the scene depth.
        float t = -1.0;
        if (cameraPosWorld.y < cloudLayerMinY) {
            if (r.y > 1e-6) t = (cloudLayerMinY - cameraPosWorld.y) / r.y;     // below clouds, looking up
        } else {
            if (r.y < -1e-6) t = (cloudLayerMaxY - cameraPosWorld.y) / r.y;    // above clouds, looking down
        }
        if (t < 0.0) {
            composite = false;
        } else {
            vec3 entry = cameraPosWorld + t * r;
            vec4 clip  = projection * view * vec4(entry, 1.0);
            float cloudDepth = clamp(clip.z / clip.w * 0.5 + 0.5, 0.0, 1.0);
            // sceneZ == 1.0 means the pixel is sky (no terrain) — always composite.
            composite = (cloudDepth <= sceneZ);
        }
    }
    // Inside the cloud layer: always composite (skip the depth-plane check —
    // it would snap to the near plane and cause blinking when the camera
    // crosses the slab boundary at speed).

    if (!composite) {
        vec3 c = hdrMode ? tonemap(sceneCol, exposure) : sceneCol;
        FragColor = vec4(applySaturation(c, saturation), 1.0);
        return;
    }

    // Recover the "pure" cloud color (un-premultiply by opacity).
    vec3 cloudRgbPure = cloud.rgb / max(cloudOpacity, 0.001);

    vec3 finalRgb;
    if (hdrMode) {
        // Blend in linear HDR, then tonemap+gamma once at the end. This keeps
        // bright cloud highlights inside the same tonemap that handles bright sun.
        vec3 hdrComposited = mix(sceneCol, cloudRgbPure, cloudOpacity);
        finalRgb = tonemap(hdrComposited, exposure);
    } else {
        // Legacy LDR: scene is already tonemapped per-shader; bring clouds to
        // LDR with the same operator and alpha-blend in LDR (preserves the
        // pre-HDR look exactly).
        vec3 cloudLdr = tonemap(cloudRgbPure, exposure);
        finalRgb = mix(sceneCol, cloudLdr, cloudOpacity);
    }

    FragColor = vec4(applySaturation(finalRgb, saturation), 1.0);
}
