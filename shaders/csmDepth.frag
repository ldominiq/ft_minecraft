
#version 460 core

in vec2 TexCoord;
flat in float TexLayer;

uniform sampler2DArray blockTextures;
uniform int cascadeIndex;

void main()
{
    // Cascade 2 (farthest) skips alpha-test: leaf-cutout shadows are
    // imperceptible at that distance and the discard kills early-Z.
    if (cascadeIndex < 2) {
        float alpha = texture(blockTextures, vec3(TexCoord, TexLayer)).a;
        if (alpha < 0.1)
            discard;
    }
}