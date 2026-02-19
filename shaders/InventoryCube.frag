#version 460 core

in vec2 vUV;
out vec4 FragColor;

uniform sampler2D atlas;

void main()
{
    vec4 tex = texture(atlas, vUV);

    // Alpha discard for transparent blocks
    if (tex.a < 0.1)
        discard;

    FragColor = tex;
}
