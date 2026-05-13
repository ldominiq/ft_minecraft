#version 460 core

// Post-terrain cloud composite.
// Reads the resolved (non-MSAA) scene color/depth and the low-res cloud RGBA texture,
// and composites clouds over the scene per-pixel using actual scene depth — no
// gl_FragDepth hacks. This kills the "milky-ground when looking down from above clouds"
// artifact and the hard cloud/terrain silhouette cutoff.

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

// Uncharted2 filmic tone mapping (same curve as sky_common.glsl::skyUncharted2).
// The cloud march outputs HDR in-scattered light; we tone-map it to LDR before
// blending so the result composites correctly into the LDR scene texture.
vec3 cloudToneMap(vec3 color, float exp_) {
    const float A=0.15, B=0.50, C=0.10, D=0.20, E=0.02, F=0.30, W=11.2, gamma=2.2;
    color *= exp_;
    color = ((color*(A*color+C*B)+D*E)/(color*(A*color+B)+D*F)) - E/F;
    float white = ((W*(A*W+C*B)+D*E)/(W*(A*W+B)+D*F)) - E/F;
    color /= white;
    return pow(max(color, vec3(0.0)), vec3(1.0/gamma));
}

void main() {
    vec3 sceneCol = texture(sceneColor, vUV).rgb;   // LDR (terrain-LDR or tone-mapped sky)
    float sceneZ  = texture(sceneDepth, vUV).r;     // [0,1] depth-buffer value
    vec4 cloud    = texture(cloudTex, vUV);         // .rgb = HDR in-scattered light, .a = transmittance
    float cloudOpacity = 1.0 - cloud.a;

    // Cloud-free pixel: pass scene through.
    if (cloudOpacity <= 0.001) {
        FragColor = vec4(sceneCol, 1.0);
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
        FragColor = vec4(sceneCol, 1.0);
        return;
    }

    // Recover the "pure" cloud color (un-premultiply by opacity), tone-map to
    // LDR, then alpha-blend over the scene. This avoids the saturation that
    // additive blending would cause in LDR space and matches the previous look
    // (clouds occlude rather than over-bright when in front of terrain).
    vec3 cloudRgbPure = cloud.rgb / max(cloudOpacity, 0.001);
    vec3 cloudLdr = cloudToneMap(cloudRgbPure, exposure);

    FragColor = vec4(mix(sceneCol, cloudLdr, cloudOpacity), 1.0);
}
