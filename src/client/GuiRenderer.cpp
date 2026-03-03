//
// Created by lucas on 10/14/25.
//

#include "GuiRenderer.hpp"


GuiRenderer::GuiRenderer(Loader loader) {
    const std::vector<float> positions = { -1, 1, -1, -1, 1, 1, 1, -1 };
    quad = loader.loadToVAO(positions);
    shader = std::make_unique<Shader>("shaders/gui.vert", "shaders/gui.frag");
}

void GuiRenderer::render(const std::vector<GuiTexture>& guis, float nearPlane, float farPlane) {
    shader->use();
    shader->setFloat("nearPlane", nearPlane);
    shader->setFloat("farPlane", farPlane);
    glBindVertexArray(quad.getVaoID());
    glEnableVertexAttribArray(0);
    glEnable(GL_BLEND); // Enable transparency
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_DEPTH_TEST); // Disable depth testing
    for (const GuiTexture& gui : guis) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, gui.getTexture());
        glm::mat4 matrix = glm::translate(glm::mat4(1.0f), glm::vec3(gui.getPosition(), 0.0f));
        matrix = glm::scale(matrix, glm::vec3(gui.getScale(), 1.0f));
        shader->setMat4("transformationMatrix", matrix);
        shader->setInt("flipY", gui.getIsFBO() ? 1 : 0);
        shader->setInt("isGrayscale", gui.getIsDepthTexture() ? 1 : 0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, quad.getVertexCount());
    }
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glDisableVertexAttribArray(0);
    glBindVertexArray(0);
    shader->stop();
}
