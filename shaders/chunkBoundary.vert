#version 460 core
layout(location = 0) in vec3 aPos;
uniform mat4 view;       // translation-free (camera at origin in render space)
uniform mat4 projection;
uniform vec3 chunkRel;   // chunkOriginWorld - eyePos, computed CPU-side in double
void main() {
    gl_Position = projection * view * vec4(chunkRel + aPos, 1.0);
}
