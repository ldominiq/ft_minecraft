#version 460 core

in vec2 TexCoord;
flat in float TexLayer;
in vec3 vFragPosRel;
flat in vec3 vNormal;
flat in float vSkyLight;
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

    // Full directional + CSM shadow + point lights — dropped items react to the
    // same lights as the terrain they're lying on.
    FragColor = vec4(entityLitColor(tex.rgb, normalize(vNormal), vFragPosRel, vSkyLight), 1.0);
}
