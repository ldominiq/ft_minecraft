#version 460 core

uniform vec3 uColor;        // fallback color when no skin is bound
uniform sampler2D uSkin;    // 2D skin texture. V=0 is top of image; no V-flip applied — UVs are already in skin-auth space.
uniform bool uUseTexture;   // when true, sample uSkin; otherwise emit uColor

in vec2 vTex;
in vec3 vFragPosRel;
in vec3 vNormal;

out vec4 FragColor;

#include "entity_lighting.glsl"

void main()
{
    vec3 albedo;
    if (uUseTexture)
    {
        vec4 c = texture(uSkin, vTex);
        if (c.a < 0.1) discard;
        albedo = c.rgb;
    }
    else
    {
        albedo = uColor;
    }
    // Full directional + CSM shadow + point lights; matches the terrain pipeline
    // (minus specular/SSAO) so mobs react to the same lighting as the world.
    FragColor = vec4(entityLitColor(albedo, normalize(vNormal), vFragPosRel), 1.0);
}
