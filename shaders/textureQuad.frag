#version 460 core
in vec2 TexCoord;
out vec4 FragColor;

uniform sampler2D uTexture;
uniform float uAlpha;

void main()
{
    vec4 col = texture(uTexture, TexCoord);
    FragColor = vec4(col.rgb, col.a * uAlpha);
}