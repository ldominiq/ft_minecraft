
#version 460 core

in vec2 TexCoord;
flat in float TexLayer;

uniform sampler2DArray blockTextures;

void main()
{
    // Discard transparent pixels (e.g. leaf cutouts) so they don't
    // write to the shadow map.  Without this, leaves cast solid
    // rectangular shadows instead of dappled ones.
    float alpha = texture(blockTextures, vec3(TexCoord, TexLayer)).a;
    if (alpha < 0.1)
        discard;
}