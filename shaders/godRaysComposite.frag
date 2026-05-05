#version 460 core
//
// Full-resolution composite for half-res god rays. Bilinear sample of the
// half-res scattering buffer, scaled by intensity, output as additive blend.
//
in vec2 TexCoords;
out vec4 FragColor;

uniform sampler2D scatterTex;
uniform float     intensity;

void main() {
    vec3 c = texture(scatterTex, TexCoords).rgb * intensity;
    // Reinhard knee — bounds the contribution so a forward-peaked Mie phase
    // at the sun direction can't add unbounded brightness on top of the
    // sky's own sun. Below ~0.5 it's near-linear; above it asymptotes to 1.
    c = c / (1.0 + c);
    FragColor = vec4(c, 1.0);
}
