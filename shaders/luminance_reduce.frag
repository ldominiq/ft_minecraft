#version 460 core

// Downsample the HDR scene into a small (e.g. 64x64) R16F target whose pixels
// contain log(luminance + eps). The CPU averages this target via PBO readback
// each frame to drive auto-exposure adaptation.
//
// We do a 3x3 jittered tap pattern to absorb noise from bright pixels (sun,
// specular highlights). The cost is trivial - 9 samples * 64 * 64 fragments.

in vec2 vUV;
out float FragColor;

uniform sampler2D hdrScene;
uniform vec2 invSrcResolution; // 1.0 / source resolution (pixel size in UV space)

const float EPS = 1e-4;

void main() {
    // Skip the screen border (helps reject letterboxed/black bars or HUD edges).
    vec2 uv = mix(vec2(0.02), vec2(0.98), vUV);

    vec3 sum = vec3(0.0);
    // 3x3 taps spaced by a few source pixels (jitters within ~3px around the bin centre).
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            vec2 off = vec2(float(dx), float(dy)) * 4.0 * invSrcResolution;
            sum += texture(hdrScene, uv + off).rgb;
        }
    }
    sum /= 9.0;

    float lum = dot(sum, vec3(0.2126, 0.7152, 0.0722));
    FragColor = log(max(lum, EPS));
}
