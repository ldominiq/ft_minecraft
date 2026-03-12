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
        // Linearize perspective depth from [0,1] non-linear to [near,far] linear,
        // then normalize to [0,1] for display.
        float depth = texel.r;
        float z = depth * 2.0 - 1.0; // [0,1] → [-1,1] NDC
        float linearDepth = (2.0 * nearPlane * farPlane) / (farPlane + nearPlane - z * (farPlane - nearPlane));
        linearDepth /= farPlane; // normalize to [0,1] for visualization
        out_Color = vec4(vec3(linearDepth), 1.0);
    } else {
        out_Color = texel;
    }
}
