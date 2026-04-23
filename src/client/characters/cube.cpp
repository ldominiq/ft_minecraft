#include "cube.hpp"

static GLuint cubeVAO = 0;
static GLuint cubeVBO = 0;
static GLuint cubeEBO = 0;

// 1x1x1 cube centered at origin, expanded to 24 unique vertices so each face
// carries its own face index and skin-UV corner. Axis convention for the
// character rig: +X = character front (face), +Y = up, +Z = character left.
//
// Face indexing is shared with the character shader and the boxUVs helper:
//   0 = +X (character front), 1 = -X (back),
//   2 = +Y (top),             3 = -Y (bottom),
//   4 = -Z (character right), 5 = +Z (character left).
//
// Each vertex layout: aPos (vec3) | aFaceIndex (float) | aFaceCorner (vec2).
// aFaceCorner is in skin-auth coords where (0,0) = top-left of the UV rect
// on the PNG and (1,1) = bottom-right. The fragment shader flips V when
// sampling, so PNG origin stays at top-left as it's authored.

void createCube()
{
    if (cubeVAO != 0) return;

    static const float vertices[] = {
        // +X face (front) — faceIndex 0 — CCW from outside (+X)
        +0.5f, -0.5f, +0.5f,  0.0f,  1.0f, 1.0f,
        +0.5f, -0.5f, -0.5f,  0.0f,  0.0f, 1.0f,
        +0.5f, +0.5f, -0.5f,  0.0f,  0.0f, 0.0f,
        +0.5f, +0.5f, +0.5f,  0.0f,  1.0f, 0.0f,

        // -X face (back) — faceIndex 1
        -0.5f, -0.5f, -0.5f,  1.0f,  1.0f, 1.0f,
        -0.5f, -0.5f, +0.5f,  1.0f,  0.0f, 1.0f,
        -0.5f, +0.5f, +0.5f,  1.0f,  0.0f, 0.0f,
        -0.5f, +0.5f, -0.5f,  1.0f,  1.0f, 0.0f,

        // +Y face (top) — faceIndex 2
        -0.5f, +0.5f, +0.5f,  2.0f,  0.0f, 0.0f,
        +0.5f, +0.5f, +0.5f,  2.0f,  0.0f, 1.0f,
        +0.5f, +0.5f, -0.5f,  2.0f,  1.0f, 1.0f,
        -0.5f, +0.5f, -0.5f,  2.0f,  1.0f, 0.0f,

        // -Y face (bottom) — faceIndex 3 (V-flipped relative to top)
        -0.5f, -0.5f, -0.5f,  3.0f,  1.0f, 1.0f,
        +0.5f, -0.5f, -0.5f,  3.0f,  1.0f, 0.0f,
        +0.5f, -0.5f, +0.5f,  3.0f,  0.0f, 0.0f,
        -0.5f, -0.5f, +0.5f,  3.0f,  0.0f, 1.0f,

        // -Z face (character right) — faceIndex 4
        -0.5f, +0.5f, -0.5f,  4.0f,  0.0f, 0.0f,
        +0.5f, +0.5f, -0.5f,  4.0f,  1.0f, 0.0f,
        +0.5f, -0.5f, -0.5f,  4.0f,  1.0f, 1.0f,
        -0.5f, -0.5f, -0.5f,  4.0f,  0.0f, 1.0f,

        // +Z face (character left) — faceIndex 5
        -0.5f, -0.5f, +0.5f,  5.0f,  1.0f, 1.0f,
        +0.5f, -0.5f, +0.5f,  5.0f,  0.0f, 1.0f,
        +0.5f, +0.5f, +0.5f,  5.0f,  0.0f, 0.0f,
        -0.5f, +0.5f, +0.5f,  5.0f,  1.0f, 0.0f,
    };

    static const unsigned int indices[] = {
        // Each face: two triangles (0,1,2) and (0,2,3) over the 4 quad verts.
         0,  1,  2,   0,  2,  3, // +X
         4,  5,  6,   4,  6,  7, // -X
         8,  9, 10,   8, 10, 11, // +Y
        12, 13, 14,  12, 14, 15, // -Y
        16, 17, 18,  16, 18, 19, // -Z
        20, 21, 22,  20, 22, 23, // +Z
    };

    glGenVertexArrays(1, &cubeVAO);
    glGenBuffers(1, &cubeVBO);
    glGenBuffers(1, &cubeEBO);

    glBindVertexArray(cubeVAO);

    // VBO
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // EBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, cubeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    const GLsizei stride = 6 * sizeof(float);

    // aPos (location 0)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);

    // aFaceIndex (location 1) — stored as float, cast to int in shader
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 1, GL_FLOAT, GL_FALSE, stride, (void*)(3 * sizeof(float)));

    // aFaceCorner (location 2) — skin-auth UV corner in [0,1]^2
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride, (void*)(4 * sizeof(float)));

    glBindVertexArray(0);
}

void drawCube()
{
    glBindVertexArray(cubeVAO);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

void destroyCube()
{
    glDeleteBuffers(1, &cubeVBO);
    glDeleteBuffers(1, &cubeEBO);
    glDeleteVertexArrays(1, &cubeVAO);
}
