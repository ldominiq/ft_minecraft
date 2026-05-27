#version 460 core
// Z-prepass fragment shader. When useAlphaTest is true, discards transparent
// texels (leaf cutouts) so the depth buffer matches what the color pass will
// produce - without this, GL_EQUAL would fail on transparent edges and leaves
// would vanish. In Fast leaf mode the host sets useAlphaTest=false, the
// fragment shader becomes a no-op, and early-Z is fully unleashed.
in vec2 TexCoord;
flat in float TexLayer;

uniform sampler2DArray blockTextures;
uniform bool useAlphaTest;

void main() {
    if (useAlphaTest) {
        float alpha = texture(blockTextures, vec3(TexCoord, TexLayer)).a;
        if (alpha < 0.1)
            discard;
    }
}
