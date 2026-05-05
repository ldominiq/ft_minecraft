#version 460 core
//
// Half-resolution screen-space volumetric god rays.
// For each pixel, march N samples from camera origin to the scene-depth
// world position. At each sample test the CSM shadow map; lit samples
// contribute Mie-phase in-scatter weighted by Beer–Lambert transmittance.
//
// Inputs (in camera-relative world space, matching lighting.frag):
//   - sceneDepth      : full-res sampleable depth, NDC z in [0,1].
//   - shadowMapArray  : CSM depth array (sampler2DArrayShadow).
//   - lightSpaceMatrices, cascadePlaneDistances, cascadeCount, viewRot : CSM uniforms,
//     populated by Lighting::uploadCSMUniforms.
//   - invProjViewRot  : inverse(projection * viewRot), reconstructs fragPosRel.
//

in vec2 TexCoords;
out vec4 FragColor;

#define MAX_CASCADES 5

uniform sampler2D            sceneDepth;
uniform sampler2DArrayShadow shadowMapArray;
uniform sampler2D            blueNoise;

uniform mat4  lightSpaceMatrices[MAX_CASCADES];
uniform float cascadePlaneDistances[MAX_CASCADES - 1];
uniform int   cascadeCount;
uniform mat4  viewRot;
uniform mat4  invProjViewRot;

uniform vec3  sunDir;       // unit, toward the sun
uniform vec3  sunColor;
uniform int   numSteps;
uniform float density;      // sigma_t == sigma_s (lumped)
uniform float anisotropy;   // Henyey-Greenstein g
uniform float maxDistance;  // clip ray length to this many units

// HG phase, normalised over the sphere (integral = 1).
float hgPhase(float mu, float g) {
    float g2 = g * g;
    float denom = pow(max(1.0 + g2 - 2.0 * g * mu, 1e-4), 1.5);
    return (1.0 - g2) / (12.566370614 * denom); // 4*pi
}

// Reconstruct camera-relative world position from screen UV + depth.
vec3 reconstructFragPosRel(vec2 uv, float depth) {
    vec4 ndc = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 p = invProjViewRot * ndc;
    return p.xyz / p.w;
}

// Sample CSM shadow at a camera-relative world-space position. Returns
// 1.0 = fully lit, 0.0 = in shadow. Mirrors the cascade-selection logic
// in lighting.frag but is parametrised by an arbitrary fragPosRel rather
// than the rasterised one.
float sampleCSMAtFragPosRel(vec3 fragPosRel) {
    if (cascadeCount == 0) return 1.0;

    vec4 viewPos = viewRot * vec4(fragPosRel, 1.0);
    float depthValue = abs(viewPos.z);

    int layer = cascadeCount - 1;
    for (int i = 0; i < cascadeCount - 1; ++i) {
        if (depthValue < cascadePlaneDistances[i]) {
            layer = i;
            break;
        }
    }

    vec4 lightPos = lightSpaceMatrices[layer] * vec4(fragPosRel, 1.0);
    vec3 proj = lightPos.xyz / lightPos.w;
    proj = proj * 0.5 + 0.5;

    if (proj.z > 1.0 ||
        proj.x < 0.0 || proj.x > 1.0 ||
        proj.y < 0.0 || proj.y > 1.0)
        return 1.0; // outside cascade → assume lit

    // sampler2DArrayShadow returns 1.0 if depth >= compareValue (lit), 0.0 otherwise.
    return texture(shadowMapArray, vec4(proj.xy, float(layer), proj.z));
}

void main() {
    float depth = texture(sceneDepth, TexCoords).r;

    // Reconstruct end-point. Sky pixels (depth==1.0) get clamped to maxDistance
    // so we still march through the volume in front of distant geometry.
    vec3 endRel;
    if (depth >= 1.0) {
        // For sky we synthesise a far point along the view ray. Use a tiny
        // depth offset to stay inside the frustum on perspective inverse.
        endRel = reconstructFragPosRel(TexCoords, 0.99999);
        float L = length(endRel);
        if (L > 0.0) endRel *= (maxDistance / L);
    } else {
        endRel = reconstructFragPosRel(TexCoords, depth);
    }

    float rayLen = min(length(endRel), maxDistance);
    if (rayLen <= 1e-3) {
        FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }
    vec3 rayDir = endRel / max(length(endRel), 1e-6);

    // Per-pixel jitter break banding. Tile the 64x64 noise across the screen.
    float jitter = texture(blueNoise, gl_FragCoord.xy / 64.0).r;

    float stepLen = rayLen / float(numSteps);
    vec3  stepVec = rayDir * stepLen;
    vec3  p       = stepVec * jitter; // camera-relative origin = vec3(0)

    float mu    = dot(rayDir, sunDir);
    float phase = hgPhase(mu, anisotropy);

    float accum = 0.0;
    float trans = 1.0;
    for (int i = 0; i < numSteps; ++i) {
        float lit = sampleCSMAtFragPosRel(p);
        accum += lit * trans * stepLen;
        trans *= exp(-density * stepLen);
        p     += stepVec;
    }

    vec3 inscatter = sunColor * phase * density * accum;
    FragColor = vec4(inscatter, 1.0);
}
