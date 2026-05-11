#version 460 core

// Fullscreen triangle, no VBO. Uses gl_VertexID with a single glDrawArrays(GL_TRIANGLES, 0, 3).
out vec2 vUV;

void main() {
    vec2 p = vec2((gl_VertexID == 1) ? 3.0 : -1.0,
                  (gl_VertexID == 2) ? 3.0 : -1.0);
    vUV = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
