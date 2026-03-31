//
// Created by lucas on 10/14/25.
//

#include "WaterRenderer.hpp"

#include "WaterFramebuffer.hpp"
#include "Camera.hpp"
#include "Renderer.hpp"
#include "Lighting.hpp"
#include "TextureManager.hpp"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>


// ============================================================
// Water Rendering Helper Methods
// ============================================================

WaterRenderer::WaterRenderer(const std::shared_ptr<Shader>& shader, const std::shared_ptr<WaterFramebuffer>& fbos) : waterShader(shader), fbos(fbos) {
    dudvTexture = shader->loadTexture("assets/textures/waterDudv.png");
    waterNormalTexture = shader->loadTexture("assets/textures/normalMap.png");

    // connect texture units
    shader->use();
    shader->setInt("reflectionTexture", 0);
    shader->setInt("refractionTexture", 1);
    shader->stop();
}

WaterRenderer::~WaterRenderer() {

}

void WaterRenderer::setDependencies(const std::shared_ptr<Lighting> &lightingRef, const std::shared_ptr<Renderer> &rendererRef, const std::shared_ptr<Camera>& cameraRef) {
    lighting = lightingRef;
    renderer = rendererRef;
    camera = cameraRef;
}

void WaterRenderer::prepareRender() {
    waterShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbos->getReflectionTexture());
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, fbos->getRefractionTexture());

}

void WaterRenderer::renderWaterReflectionPass(const std::shared_ptr<Shader> &sceneShader, const glm::mat4& projection, const TextureManager& texMgr) {
    fbos->bindReflectionFrameBuffer();
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_CLIP_DISTANCE0);

    // Calculate reflected view matrix
    float distance = 2.0f * (camera->getPlayer()->getPosition().y - seaLevel);
    glm::vec3 reflectCamPos = camera->getPlayer()->getPosition();
    reflectCamPos.y -= distance;

    // Construct reflected view matrix with inverted pitch
    const float yaw = camera->getPlayer()->getYaw();
    const float pitch = -camera->getPlayer()->getPitch();  // Inverted pitch for reflection

    glm::vec3 front{};
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(front);

    const glm::mat4 reflectView = glm::lookAt(reflectCamPos, reflectCamPos + front, glm::vec3(0, 1, 0));
    distance = 2.0f * (camera->getPlayer()->getPosition().y - seaLevel);
    reflectCamPos = camera->getPlayer()->getPosition();
    reflectCamPos.y -= distance;

    // Set clip plane (only render above water)
    const glm::vec4 clipPlane = glm::vec4(0, 1, 0, -(seaLevel));

    sceneShader->use();
    sceneShader->setVec4("clipPlane", clipPlane);
    sceneShader->setMat4("view", reflectView);
    sceneShader->setMat4("projection", projection);

    // Calculate reflected camera direction for lighting
    const glm::vec3 originalDir = camera->getPlayer()->getCameraDir();
    const glm::vec3 reflectedDir = glm::vec3(originalDir.x, -originalDir.y, originalDir.z);

    lighting->uploadLightingUniforms(*sceneShader, reflectCamPos, reflectedDir);
    // Skip uploadCSMUniforms: it binds csmDepthMaps which was just written by the shadow pass
    // milliseconds ago — binding it for reading here causes an implicit driver sync stall.
    sceneShader->setInt("ssaoEnabled", 0);
    sceneShader->setFloat("shadows.enabled", 0.0f);
    // Render reflection scene
    texMgr.bind(GL_TEXTURE0);
    constexpr glm::mat4 skyView = glm::mat4(-1.0);
    lighting->drawSky(skyView, projection, reflectCamPos, false);
    // Update vegetation shader with reflected view/clip before rendering
    renderer->updateVegetationUniforms(reflectView, projection, clipPlane, reflectCamPos);
    // Use the reflected view-projection for frustum culling so only chunks
    // actually visible in the reflection are submitted, not all main-camera chunks.
    renderer->updateFrustum(projection * reflectView);
    renderer->render(sceneShader, false); // skip vegetation

    glDisable(GL_CLIP_DISTANCE0);
    fbos->unbindCurrentFrameBuffer();
    // Restore main-camera frustum for all subsequent passes this frame.
    renderer->updateFrustum(projection * camera->getViewMatrix());
}

void WaterRenderer::renderWaterRefractionPass(const std::shared_ptr<Shader>& sceneShader, const glm::mat4& view, const glm::mat4& projection, const TextureManager& texMgr) {
    fbos->bindRefractionFrameBuffer();
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
    lighting->uploadLightingUniforms(*sceneShader, camera->getPlayer()->getPosition(), camera->getPlayer()->getCameraDir());
    // Skip uploadCSMUniforms: same shadow texture hazard as reflection — and underwater
    // fragments don't need shadow computation at all.
    sceneShader->setInt("ssaoEnabled", 0);
    sceneShader->setFloat("shadows.enabled", 0.0f);
    texMgr.bind(GL_TEXTURE0);
    // Render with vegetation so sea vegetation is visible in the refraction texture
    renderer->updateVegetationUniforms(view, projection, clipPlane, camera->getPlayer()->getPosition());
    renderer->render(sceneShader, true);

    glDisable(GL_CLIP_DISTANCE0);
    fbos->unbindCurrentFrameBuffer();
}

void WaterRenderer::renderWaterSurface(const glm::mat4& projection) {
    prepareRender();

    const glm::mat4 view = camera->getViewMatrix();
    const glm::vec3 camPos = camera->getPlayer()->getPosition();

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

    glDisable(GL_BLEND);
}
