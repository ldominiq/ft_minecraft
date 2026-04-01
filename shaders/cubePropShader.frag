#version 460 core

in vec2 TexCoord;
flat in float TexLayer;
out vec4 FragColor;

uniform sampler2DArray blockTextures;

void main()
{
    vec4 tex = texture(blockTextures, vec3(TexCoord, TexLayer));

    // Alpha discard for transparent blocks
    if (tex.a < 0.1)
        discard;

    // Unpremultiply alpha to get original colors (only for semi-transparent pixels)
    // For opaque or nearly-opaque pixels (alpha > 0.95), skip to avoid precision issues
    if (tex.a > 0.01 && tex.a < 0.95) {
        tex.rgb /= tex.a;
    }

    FragColor = vec4(tex.rgb, 1.0);
}
