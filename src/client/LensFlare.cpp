#include "LensFlare.hpp"

#include <glm/gtc/type_ptr.hpp>

LensFlare::LensFlare() {
    flareShader = std::make_unique<Shader>(
        "shaders/lensFlare.vert", "shaders/lensFlare.frag");
    glGenVertexArrays(1, &quadVAO);
}

LensFlare::~LensFlare() {
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
}

void LensFlare::render(const glm::mat4& viewProj,
                       const glm::vec3& sunDirToward,
                       GLuint sceneDepthTex,
                       int screenWidth,
                       int screenHeight)
{
    if (!enabled || intensity <= 0.0f) return;

    // Project the sun out to a far point in clip space. If it ends up
    // behind the camera or far outside the screen we skip the draw — the
    // shader would also early-out, but skipping the draw is cheaper.
    const glm::vec4 sunClip = viewProj * glm::vec4(sunDirToward * 1e6f, 1.0f);
    if (sunClip.w <= 0.0f) return;
    const glm::vec3 sunNDC = glm::vec3(sunClip) / sunClip.w;
    if (std::abs(sunNDC.x) > 1.3f || std::abs(sunNDC.y) > 1.3f) return;
    const glm::vec2 sunUV(sunNDC.x * 0.5f + 0.5f, sunNDC.y * 0.5f + 0.5f);

    flareShader->use();
    flareShader->setVec2("sunUV", sunUV);
    flareShader->setVec2("screenSize",
                         glm::vec2(static_cast<float>(screenWidth),
                                   static_cast<float>(screenHeight)));
    flareShader->setFloat("intensity", intensity);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTex);
    flareShader->setInt("sceneDepth", 0);

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
}
