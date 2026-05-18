#version 460 core

in vec2 TexCoord;
flat in float TexLayer;
in vec3 vFragPosRel;
flat in vec3 vNormal;
flat in float vSkyLight;
flat in float vBlockLight;
out vec4 FragColor;

uniform sampler2DArray blockTextures;

#include "entity_lighting.glsl"

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

    // vSkyLight > 2.5 is the self-lit torch sentinel (the torch item itself
    // glows). Otherwise it's a normal 0..1 skylight and vBlockLight carries
    // baked torch light independently (added as a warm sky/shadow-independent
    // term inside entityLitColor — same as terrain/mobs).
    if (vSkyLight > 2.5) {
        FragColor = vec4(tex.rgb, 1.0);
        return;
    }

    FragColor = vec4(entityLitColor(tex.rgb, normalize(vNormal), vFragPosRel,
                                    clamp(vSkyLight, 0.0, 1.0), vBlockLight), 1.0);
}
