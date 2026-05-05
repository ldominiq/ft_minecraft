#version 460 core

layout (location = 0) in vec3 aPos;
layout (location = 1) in float aFaceIndex;
layout (location = 2) in vec2 aFaceCorner;

uniform mat4 uMVP;
// Per-part face UV rects in normalized skin-auth space (V=0 is top of PNG).
// xy = (u_left, v_top), zw = (u_right, v_bottom). Indexed 0..5.
uniform vec4 uFaceUVs[6];

out vec2 vTex;

void main()
{
    int face = int(aFaceIndex + 0.5);
    vec4 rect = uFaceUVs[face];
    vTex = mix(rect.xy, rect.zw, aFaceCorner);

    gl_Position = uMVP * vec4(aPos, 1.0);
}
