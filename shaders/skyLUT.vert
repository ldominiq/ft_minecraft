#version 460 core

// Fullscreen triangle using gl_VertexID. No attributes needed.
void main() {
    vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    vec2 ndc = pos * 2.0 - 1.0;
    gl_Position = vec4(ndc, 0.0, 1.0);
}
