#include "SkyLUT.hpp"
#include <iostream>
#include <cmath>

SkyLUT::SkyLUT(int lutWidth, int lutHeight)
    : LUT_WIDTH(lutWidth), LUT_HEIGHT(lutHeight)
{
    lutShader = std::make_unique<Shader>("shaders/skyLUT.vert", "shaders/skyLUT.frag");
    glGenVertexArrays(1, &quadVAO);
    createResources();
}

SkyLUT::~SkyLUT() {
    destroyResources();
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
}

void SkyLUT::createResources() {
    // LUT texture - RGBA16F for Rayleigh (RGB) + Mie (A)
    glGenTextures(1, &lutTexture);
    glBindTexture(GL_TEXTURE_2D, lutTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, LUT_WIDTH, LUT_HEIGHT, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // FBO
    glGenFramebuffers(1, &lutFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, lutFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, lutTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::SKYLUT::FRAMEBUFFER_NOT_COMPLETE" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SkyLUT::destroyResources() {
    if (lutFBO) {
        glDeleteFramebuffers(1, &lutFBO);
        lutFBO = 0;
    }
    if (lutTexture) {
        glDeleteTextures(1, &lutTexture);
        lutTexture = 0;
    }
}

bool SkyLUT::update(float atmDensity, float atmThickness, float cameraPosY, float seaLevel, float planetScale, int viewportWidth, int viewportHeight) {
    // Check if anything changed (use tolerance for camera height to avoid constant regeneration)
    const float heightTolerance = 1.0f; // regenerate every ~1 world units of vertical movement
    if (!dirty &&
        atmDensity == cachedAtmDensity &&
        atmThickness == cachedAtmThickness &&
        std::abs(cameraPosY - cachedCameraPosY) < heightTolerance &&
        seaLevel == cachedSeaLevel &&
        planetScale == cachedPlanetScale) {
        return false;
    }

    // Cache current parameters
    cachedAtmDensity = atmDensity;
    cachedAtmThickness = atmThickness;
    cachedCameraPosY = cameraPosY;
    cachedSeaLevel = seaLevel;
    cachedPlanetScale = planetScale;
    dirty = false;

    // Render the LUT
    glBindFramebuffer(GL_FRAMEBUFFER, lutFBO);
    glViewport(0, 0, LUT_WIDTH, LUT_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT);

    lutShader->use();
    lutShader->setVec2("lutSize", glm::vec2(LUT_WIDTH, LUT_HEIGHT));
    lutShader->setFloat("atmDensity", atmDensity);
    lutShader->setFloat("atmThickness", atmThickness);
    lutShader->setFloat("cameraPosY", cameraPosY);
    lutShader->setFloat("seaLevel", seaLevel);
    lutShader->setFloat("planetScale", planetScale);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Restore the caller's viewport
    glViewport(0, 0, viewportWidth, viewportHeight);

    return true;
}
