#version 460 core

// Fullscreen triangle using gl_VertexID. No attributes needed.
void main() {
    // Generate positions in [0,1]: (0,0), (2,0)->(1,0), (0,2)->(0,1)
    vec2 pos = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    // Map to NDC [-1,1]
    vec2 ndc = pos * 2.0 - 1.0;
    // Set depth to far plane (z=w means depth=1.0 after perspective division)
    // This ensures sky renders behind everything else
    gl_Position = vec4(ndc, 1.0, 1.0);
}
