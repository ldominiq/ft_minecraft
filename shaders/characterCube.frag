#version 460 core

uniform vec3 uColor;        // fallback color when no skin is bound
uniform sampler2D uSkin;    // 2D skin texture, authored top-left origin
uniform bool uUseTexture;   // when true, sample uSkin; otherwise emit uColor

in vec2 vTex;

out vec4 FragColor;

void main()
{
    if (uUseTexture)
    {
        vec4 c = texture(uSkin, vTex);
        if (c.a < 0.1) discard;
        FragColor = vec4(c.rgb, 1.0);
    }
    else
    {
        FragColor = vec4(uColor, 1.0);
    }
}
