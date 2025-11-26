//
// Created by lucas on 10/14/25.
//

#include "WaterRenderer.hpp"

#include "WaterFramebuffer.hpp"
#include "Camera.hpp"
#include "Renderer.hpp"
#include "Lighting.hpp"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>


// ============================================================
// Water Rendering Helper Methods
// ============================================================

WaterRenderer::WaterRenderer(const std::shared_ptr<Shader>& shader, const std::shared_ptr<WaterFramebuffer>& fbos) : waterShader(shader), fbos(fbos) {
    // if (overlayVAO == 0) {
    //     // Full-screen quad covering NDC [-1,1]
    //     const float quad[] = {
    //         // pos.xy   // uv
    //         -1.0f, -1.0f, 0.0f, 0.0f,
    //          1.0f, -1.0f, 1.0f, 0.0f,
    //          1.0f,  1.0f, 1.0f, 1.0f,
    //
    //         -1.0f, -1.0f, 0.0f, 0.0f,
    //          1.0f,  1.0f, 1.0f, 1.0f,
    //         -1.0f,  1.0f, 0.0f, 1.0f
    //     };
    //     glGenVertexArrays(1, &overlayVAO);
    //     glGenBuffers(1, &overlayVBO);
    //     glBindVertexArray(overlayVAO);
    //     glBindBuffer(GL_ARRAY_BUFFER, overlayVBO);
    //     glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    //     glEnableVertexAttribArray(0);
    //     glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    //     glEnableVertexAttribArray(1);
    //     glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    //     glBindVertexArray(0);
    // }

    // connect texture units
    shader->use();
    shader->setInt("reflectionTexture", 0);
    shader->setInt("refractionTexture", 1);
    shader->stop();
}

WaterRenderer::~WaterRenderer() {

}

void WaterRenderer::setDependencies(
                                     GLuint dudvTex, GLuint waterNormalTex) {
    dudvTexture = dudvTex;
    waterNormalTexture = waterNormalTex;
}

void WaterRenderer::prepareRender() {
    waterShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbos->getReflectionTexture());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, fbos->getRefractionTexture());

}

glm::mat4 WaterRenderer::calculateReflectedViewMatrix(const Camera& camera, float seaLevel) const {
    // Calculate reflected camera position
    const float distance = 2.0f * (camera.movement.getPosition().y - seaLevel);
    glm::vec3 reflectCamPos = camera.movement.getPosition();
    reflectCamPos.y -= distance;

    // Construct reflected view matrix with inverted pitch
    const float yaw = camera.movement.getYaw();
    const float pitch = -camera.movement.getPitch();  // Inverted pitch for reflection

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(front);

    return glm::lookAt(reflectCamPos, reflectCamPos + front, glm::vec3(0, 1, 0));
}

void WaterRenderer::renderWaterReflectionPass(const std::shared_ptr<Lighting> &lighting, const std::shared_ptr<Renderer> &renderer, const std::shared_ptr<Shader>& sceneShader, const std::shared_ptr<Camera>& camera, const glm::mat4& projection, float seaLevel, unsigned int tex) {
    // waterFramebuffer->bindReflectionFrameBuffer();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // glEnable(GL_CLIP_DISTANCE0);

    // Calculate reflected view matrix
    // Calculate reflected camera position


    float distance = 2.0f * (camera->movement.getPosition().y - seaLevel);
    glm::vec3 reflectCamPos = camera->movement.getPosition();
    reflectCamPos.y -= distance;

    // Construct reflected view matrix with inverted pitch
    const float yaw = camera->movement.getYaw();
    const float pitch = -camera->movement.getPitch();  // Inverted pitch for reflection

    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(front);

    const glm::mat4 reflectView = glm::lookAt(reflectCamPos, reflectCamPos + front, glm::vec3(0, 1, 0));
    distance = 2.0f * (camera->movement.getPosition().y - seaLevel);
    reflectCamPos = camera->movement.getPosition();
    reflectCamPos.y -= distance;

    // Set clip plane (only render above water) with adaptive bias
    const float camHeightToWater = camera->movement.getPosition().y - seaLevel;
    const float clipBias = glm::clamp(std::abs(camHeightToWater) * 0.02f, 0.02f, 0.5f);
    const glm::vec4 clipPlane = glm::vec4(0, 1, 0, -(seaLevel));

    sceneShader->use();
    sceneShader->setVec4("clipPlane", clipPlane);
    sceneShader->setMat4("view", reflectView);
    sceneShader->setMat4("projection", projection);

    // Calculate reflected camera direction for lighting
    const glm::vec3 originalDir = camera->movement.getCameraDir();
    const glm::vec3 reflectedDir = glm::vec3(originalDir.x, -originalDir.y, originalDir.z);

    lighting->uploadLightingUniforms(*sceneShader, reflectCamPos, reflectedDir);
    // Render reflection scene
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    // lighting->drawSky(reflectView, projection, reflectCamPos);
    renderer->render(sceneShader);
    glDisable(GL_CLIP_DISTANCE0);

    // Unbind the FBO, binding the default framebuffer (ID 0)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void WaterRenderer::renderWaterRefractionPass(const std::shared_ptr<Lighting> &lighting, const std::shared_ptr<Camera> &camera, const std::shared_ptr<Renderer> &renderer, const std::shared_ptr<Shader>& sceneShader, const glm::mat4& view, const glm::mat4& projection, float seaLevel, unsigned int tex) {
    // bindRefractionFrameBuffer();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_CLIP_DISTANCE0);

    // Clip everything ABOVE the water (y > seaLevel), i.e., keep fragments below the plane y = seaLevel
    // The plane is (0, -1, 0, D), where D is seaLevel
    const glm::vec4 clipPlane = glm::vec4(0, -1, 0, seaLevel);

    sceneShader->use();
    sceneShader->setVec4("clipPlane", clipPlane);
    sceneShader->setMat4("view", view);
    sceneShader->setMat4("projection", projection);

    // Render refraction scene
    lighting->uploadLightingUniforms(*sceneShader, camera->movement.getPosition(), camera->movement.getCameraDir());
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    renderer->render(sceneShader);

    // unbindCurrentFrameBuffer();
    // glDisable(GL_CLIP_DISTANCE0);


}

void WaterRenderer::renderWaterSurface(const std::shared_ptr<Lighting> &lighting, const std::shared_ptr<Renderer> &renderer, const std::shared_ptr<Camera> &camera, const glm::mat4& projection, float seaLevel) {
    prepareRender();
    // Configure OpenGL state for transparent water rendering
    // glEnable(GL_DEPTH_TEST);
    // glEnable(GL_BLEND);
    // glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // glDepthMask(GL_FALSE);  // Disable depth writes for transparency

    // Configure face culling based on camera position
    // const GLboolean cullWasEnabled = glIsEnabled(GL_CULL_FACE);
    // const glm::vec3 camPos = camera->movement.getPosition();
    // const glm::ivec3 eyeBlock = glm::ivec3(glm::floor(camPos));
    // const bool isUnderwater = (renderer->getBlockWorld(eyeBlock) == BlockType::WATER);
    // const bool belowSeaSurface = camPos.y < (seaLevel - 0.05f);
    //
    // GLint prevCullFaceMode = GL_BACK;
    // glGetIntegerv(GL_CULL_FACE_MODE, &prevCullFaceMode);
    //
    // if (isUnderwater && belowSeaSurface) {
    //     if (cullWasEnabled) glDisable(GL_CULL_FACE);
    // } else {
    //     if (!cullWasEnabled) glEnable(GL_CULL_FACE);
    //     glCullFace(GL_BACK);
    // }

    const glm::mat4 view = camera->getViewMatrix();
    const glm::vec3 camPos = camera->movement.getPosition();

    // Set water shader uniforms
    waterShader->use();
    waterShader->setMat4("projection", projection);
    waterShader->setMat4("view", view);
    waterShader->setVec3("cameraPos", camPos);
    waterShader->setVec3("lightColor", lighting->getDirectionalDiffuseColor());
    waterShader->setVec3("lightPosition", lighting->getLightPos());
    // Horizon threshold for specular cutoff (sun below horizon → no specular)
    waterShader->setFloat("horizonY", 55.0f);
    waterShader->setFloat("twilightBand", 8.0f); // smooth fade band around horizon (units of world Y)
    waterShader->setFloat("moveFactor", waterMoveFactor);
    waterShader->setFloat("waveStrength", waveStrength);
    waterShader->setFloat("tiling", dudvTiling);
    waterShader->setFloat("nearPlane", 0.1f);
    waterShader->setFloat("farPlane", 1000.0f);

    // Bind water textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbos->getReflectionTexture());
    waterShader->setInt("reflectionTexture", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, fbos->getRefractionTexture());
    waterShader->setInt("refractionTexture", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, dudvTexture);
    waterShader->setInt("dudvMap", 2);

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, waterNormalTexture);
    waterShader->setInt("normalMap", 3);

    glActiveTexture(GL_TEXTURE4);
    glBindTexture(GL_TEXTURE_2D, fbos->getRefractionDepthTexture());
    waterShader->setInt("refractionDepthTexture", 4);

    // Enable alpha blending
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Render water meshes
    renderer->renderWater();

    // Restore OpenGL state
    // if (cullWasEnabled) {
    //     glEnable(GL_CULL_FACE);
    // } else {
    //     glDisable(GL_CULL_FACE);
    // }
    // glCullFace(prevCullFaceMode);
    // glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);

	// renderUnderWater();
}
/*
void WaterRenderer::renderUnderWater() {
    const glm::vec3 camPosLocal = camera->movement.getPosition();
    const glm::ivec3 eyeBlock = glm::ivec3(glm::floor(camPosLocal));
    const bool isUnder = (renderer->getBlockWorld(eyeBlock) == BlockType::WATER);
    if (isUnder && underwaterOverlayShader && overlayVAO) {
        const GLboolean depthWasEnabled = glIsEnabled(GL_DEPTH_TEST);
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        underwaterOverlayShader->use();
        underwaterOverlayShader->setFloat("uTime", static_cast<float>(glfwGetTime()));
        underwaterOverlayShader->setVec2("uResolution", glm::vec2(screenWidth, screenHeight));

        // Bind scene textures from refraction FBO and dudv for distortion
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, getRefractionTexture());
        underwaterOverlayShader->setInt("uSceneColor", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, waterRenderer->getRefractionDepthTexture());
        underwaterOverlayShader->setInt("uSceneDepth", 1);
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, dudvTexture);
        underwaterOverlayShader->setInt("uDuDv", 2);

        underwaterOverlayShader->setFloat("uMove", waterMoveFactor);
        underwaterOverlayShader->setFloat("uNear", 0.1f);
        underwaterOverlayShader->setFloat("uFar", renderDistance);
        underwaterOverlayShader->setVec3("uFogColor", glm::vec3(0.05f, 0.35f, 0.55f));
        underwaterOverlayShader->setFloat("uFogDensity", 0.085f);
        underwaterOverlayShader->setVec3("uTintColor", glm::vec3(0.9f, 1.05f, 1.1f));
        underwaterOverlayShader->setFloat("uOpacity", 0.42f);

        glBindVertexArray(overlayVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindVertexArray(0);

        glDisable(GL_BLEND);
        if (depthWasEnabled) glEnable(GL_DEPTH_TEST);
    }
}

*/