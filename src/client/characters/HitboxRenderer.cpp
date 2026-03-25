#include "HitboxRenderer.hpp"

HitboxRenderer::HitboxRenderer() {
    shader = std::make_shared<Shader>("shaders/simpleWireframe.vert", "shaders/simpleWireframe.frag");

    // 12 edges, 2 vertices per edge => 24 vertices
    static const float lines[24 * 3] = {
        // Bottom square (y = -0.5)
        -0.5f, -0.5f, -0.5f,  0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f, -0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f, -0.5f, -0.5f, -0.5f,
        // Top square (y = +0.5)
        -0.5f,  0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,  0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f, -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f, -0.5f,  0.5f, -0.5f,
        // Vertical edges
        -0.5f, -0.5f, -0.5f, -0.5f,  0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,  0.5f,  0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,  0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f, -0.5f,  0.5f,  0.5f
    };

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(lines), lines, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

HitboxRenderer::~HitboxRenderer() {
    if (vbo) glDeleteBuffers(1, &vbo);
    if (vao) glDeleteVertexArrays(1, &vao);
}

void HitboxRenderer::drawAABB(const AABB& box, const glm::mat4& view, const glm::mat4& proj, const glm::vec3& color) {
    if (!shader) return;
    shader->use();

    // compute model from AABB
    glm::vec3 size = box.max - box.min;
    glm::vec3 center = (box.min + box.max) * 0.5f;

    glm::mat4 model(1.0f);
    model = glm::translate(model, center);
    model = glm::scale(model, size); // unit cube [-0.5,0.5] scaled to size

    shader->setMat4("model", model);
    shader->setMat4("view", view);
    shader->setMat4("projection", proj);
    shader->setVec3("color", color);

    glBindVertexArray(vao);
    glDrawArrays(GL_LINES, 0, 24);
    glBindVertexArray(0);
}