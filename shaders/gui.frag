#version 460 core

in vec2 textureCoords;

out vec4 out_Color;

uniform sampler2D guiTexture;
uniform int isGrayscale;
uniform float nearPlane;
uniform float farPlane;

void main(void){
    vec4 texel = texture(guiTexture, textureCoords);
    if (isGrayscale == 1) {
        // Linearize the depth value from [0,1] non-linear to [0,1] linear
        float depth = texel.r;
        float linearDepth = (2.0 * nearPlane) / (farPlane + nearPlane - depth * (farPlane - nearPlane));
        out_Color = vec4(vec3(linearDepth), 1.0);
    } else {
        out_Color = texel;
    }
}
