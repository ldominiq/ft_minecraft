#version 460 core
// Z-prepass fragment shader. Discards transparent texels (leaf cutouts) so
// the depth buffer matches what the color pass will produce — without this,
// GL_EQUAL would fail on transparent edges and leaves would vanish.
in vec2 TexCoord;
flat in float TexLayer;

uniform sampler2DArray blockTextures;

void main() {
    float alpha = texture(blockTextures, vec3(TexCoord, TexLayer)).a;
    if (alpha < 0.1)
        discard;
}
