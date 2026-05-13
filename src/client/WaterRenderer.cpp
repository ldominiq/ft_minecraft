//
// Created by lucas on 10/14/25.
//

#include "WaterRenderer.hpp"
#include "FogUniforms.hpp"

#include "WaterFramebuffer.hpp"
#include "Camera.hpp"
#include "Renderer.hpp"
#include "Lighting.hpp"
#include "TextureManager.hpp"
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>
#include <algorithm>


// ============================================================
// Water Rendering Helper Methods
// ============================================================

WaterRenderer::WaterRenderer(int screenWidth, int screenHeight) {

    fbos = std::make_unique<WaterFramebuffer>(screenWidth, screenHeight);
    waterShader = std::make_unique<Shader>("shaders/water.vert", "shaders/water.frag");
    // Sky-reflection shader for water that isn't on the planar (sea-level)
    // plane — placed buckets, spread water, exposed deep-ocean side faces.
    placedWaterShader = std::make_unique<Shader>("shaders/water_placed.vert", "shaders/water_placed.frag");
    dudvTexture = waterShader->loadTexture("assets/textures/waterDudv.png");
    waterNormalTexture = waterShader->loadTexture("assets/textures/normalMap.png");

	// override default GL_NEAREST filter set by loadTexture() for these two textures
    // Override to trilinear + max-8 anisotropy on just these two textures.
    auto setSmoothFilter = [](GLuint tex) {
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    #if defined(GL_TEXTURE_MAX_ANISOTROPY_EXT)
        float maxAniso = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, std::min(8.0f, maxAniso));
    #elif defined(GL_TEXTURE_MAX_ANISOTROPY)
        float maxAniso = 0.0f;
        glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY, &maxAniso);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY, std::min(8.0f, maxAniso));
    #endif
    };
    setSmoothFilter(dudvTexture);
    setSmoothFilter(waterNormalTexture);
    glBindTexture(GL_TEXTURE_2D, 0);

    // connect texture units
    waterShader->use();
    waterShader->setInt("reflectionTexture", 0);
    waterShader->setInt("refractionTexture", 1);
    waterShader->stop();
}

WaterRenderer::~WaterRenderer() {

}

void WaterRenderer::setDependencies(const std::shared_ptr<Lighting> &lightingRef, const std::shared_ptr<Renderer> &rendererRef, const std::shared_ptr<Camera>& cameraRef) {
    lighting = lightingRef;
    renderer = rendererRef;
    camera = cameraRef;
}

void WaterRenderer::advanceWaveTime(float dt) {
    // Wrap at 2π * 1024 — large enough that the wrap is invisible (cos is
    // periodic in 2π), small enough that float precision stays good.
    constexpr float kTwoPi = 6.28318530717958647692f;
    constexpr float kWrap  = kTwoPi * 1024.0f;
    waveTime += dt;
    if (waveTime > kWrap) waveTime -= kWrap;
}

void WaterRenderer::setRefractionResolutionScale(float scale, int displayWidth, int displayHeight) {
    if (scale < 0.1f) scale = 0.1f;
    if (scale > 1.0f) scale = 1.0f;
    refractionResolutionScale = scale;

    // Recreate the refraction FBO at the new size — one-shot, not per frame.
    const int w = static_cast<int>(static_cast<float>(displayWidth)  * scale);
    const int h = static_cast<int>(static_cast<float>(displayHeight) * scale);
    fbos->resizeRefraction(w, h);
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
    glm::dvec3 reflectCamPosD = camera->getEyePosD();
    const double distance = 2.0 * (reflectCamPosD.y - static_cast<double>(seaLevel));
    reflectCamPosD.y -= distance;
    glm::vec3 reflectCamPos = glm::vec3(reflectCamPosD);

    // Construct reflected view matrix with inverted pitch
    const float yaw = camera->getPlayer()->getYaw();
    const float pitch = -camera->getPlayer()->getPitch();  // Inverted pitch for reflection

    glm::dvec3 front{};
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    front = glm::normalize(front);

    const glm::mat4 reflectView = glm::mat4(glm::lookAt(reflectCamPosD, reflectCamPosD + front, glm::dvec3(0.0, 1.0, 0.0)));

    // Set clip plane (only render above water)
    const glm::vec4 clipPlane = glm::vec4(0, 1, 0, -(seaLevel));

    sceneShader->use();
    sceneShader->setVec4("clipPlane", clipPlane);
    sceneShader->setMat4("view", reflectView);
    sceneShader->setMat4("projection", projection);
    sceneShader->setBool("useAlphaTest",
        ChunkRenderer::sLeafRenderMode != ChunkRenderer::LeafRenderMode::Fast);

    // Calculate reflected camera direction for lighting
    const glm::vec3 originalDir = camera->getPlayer()->getCameraDir();
    const glm::vec3 reflectedDir = glm::vec3(originalDir.x, -originalDir.y, originalDir.z);

    lighting->uploadLightingUniforms(*sceneShader, reflectCamPosD, reflectedDir);
    // Skip uploadCSMUniforms: it binds csmDepthMaps which was just written by the shadow pass
    // milliseconds ago — binding it for reading here causes an implicit driver sync stall.
    sceneShader->setInt("ssaoEnabled", 0);
    sceneShader->setFloat("shadows.enabled", 0.0f);
    // Render reflection scene
    texMgr.bind(GL_TEXTURE0);
    constexpr glm::mat4 skyView = glm::mat4(-1.0);
    lighting->drawSky(skyView, projection, reflectCamPos, false);

    if (reflectionEnabled) {
        // Update vegetation shader with reflected view/clip before rendering
        renderer->updateVegetationUniforms(reflectView, projection, clipPlane, reflectCamPos);
        // Use the reflected view-projection for frustum culling so only chunks
        // actually visible in the reflection are submitted, not all main-camera chunks.
        glm::mat4 reflectViewRot = reflectView;
        reflectViewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        renderer->updateFrustum(projection * reflectViewRot, reflectCamPosD);

        // Optionally cap the per-chunk render distance for reflection only —
        // distant terrain rarely contributes meaningfully to a reflection but
        // costs the same draw-call/vertex work as the main pass.
        const float prevDistCap = renderer->getMaxRenderDistanceOverride();
        if (reflectionMaxDistance > 0.0f)
            renderer->setMaxRenderDistanceOverride(reflectionMaxDistance);
        renderer->render(sceneShader, reflectView, reflectCamPosD, false); // skip vegetation
        renderer->setMaxRenderDistanceOverride(prevDistCap);
    }
    // When reflection is disabled we still ran drawSky() above so the FBO
    // contains a usable sky-tinted image — water surface will sample it as
    // a plain reflection of the sky, which is cheap and looks fine.

    glDisable(GL_CLIP_DISTANCE0);
    fbos->unbindCurrentFrameBuffer();
    // Restore main-camera frustum for all subsequent passes this frame.
  glm::mat4 mainView = camera->getViewMatrix();
    glm::mat4 mainViewRot = mainView;
    mainViewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    renderer->updateFrustum(projection * mainViewRot, camera->getEyePosD());
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
    sceneShader->setBool("useAlphaTest",
        ChunkRenderer::sLeafRenderMode != ChunkRenderer::LeafRenderMode::Fast);

    // Render refraction scene
    lighting->uploadLightingUniforms(*sceneShader, camera->getEyePosD(), camera->getPlayer()->getCameraDir());
    // Skip uploadCSMUniforms: same shadow texture hazard as reflection — and underwater
    // fragments don't need shadow computation at all.
    sceneShader->setInt("ssaoEnabled", 0);
    sceneShader->setFloat("shadows.enabled", 0.0f);
    texMgr.bind(GL_TEXTURE0);
    // Sea vegetation in refraction is expensive in dense biomes — toggleable.
    if (refractionRendersVegetation)
        renderer->updateVegetationUniforms(view, projection, clipPlane, glm::vec3(camera->getEyePosD()));
    renderer->render(sceneShader, view, camera->getEyePosD(), refractionRendersVegetation);

    glDisable(GL_CLIP_DISTANCE0);
    fbos->unbindCurrentFrameBuffer();
}

void WaterRenderer::setupSurfaceShader(Shader& shader,
                                       const glm::mat4& projection,
                                       const glm::mat4& viewRot,
                                       const glm::dvec3& eyePosD,
                                       const glm::vec3& sunDir) {
    // Anchor the dudv texture coordinate to an eye-relative origin snapped
    // to a multiple of the texture's repeat period (1/tiling). Lets the
    // vertex shader build texture coords from camera-relative positions
    // without losing wave detail at large world coordinates.
    //
    // Y is also anchored: side faces of placed-water blocks use Y for V,
    // and without this the texture would translate vertically as the eye
    // moves up/down (since cameraRelPos.y = worldY - eye.y). The ocean
    // shader only uses XZ and ignores the Y component — uniform is a
    // silent no-op there.
    const double period = (dudvTiling > 0.0f) ? (1.0 / static_cast<double>(dudvTiling)) : 1.0;
    const double anchorX = std::floor(eyePosD.x / period) * period;
    const double anchorZ = std::floor(eyePosD.z / period) * period;
    const double anchorY = std::floor(eyePosD.y / period) * period;
    const glm::vec2 texAnchor(static_cast<float>(eyePosD.x - anchorX),
                              static_cast<float>(eyePosD.z - anchorZ));
    const float texAnchorY = static_cast<float>(eyePosD.y - anchorY);

    shader.use();
    shader.setMat4("projection", projection);
    shader.setMat4("viewRot", viewRot);
    // Sun direction (toward-sun convention, matching dirLight). Set
    // explicitly — uploadFogUniforms only writes sunDir when fog is on.
    shader.setVec3("sunDir", sunDir);
    shader.setVec3("lightColor", lighting->getDirectionalDiffuseColor());
    // Sun-elevation fade band for specular (sunDir.y in [0.235, 0.315]
    // matches the old lightPos.y in [55±8] behavior).
    shader.setFloat("twilightLow",  0.235f);
    shader.setFloat("twilightHigh", 0.315f);
    shader.setVec2("texAnchor", texAnchor);
    shader.setFloat("texAnchorY", texAnchorY);
    shader.setFloat("moveFactor", waterMoveFactor);
    shader.setFloat("moveFactor2", waterMoveFactor2);
    shader.setFloat("waveStrength", waveStrength);
    shader.setFloat("tiling", dudvTiling);

    // Gerstner wave inputs. waveAnchor wraps the eye to a 1024-unit period
    // so the cos/sin argument stays in f32 range while staying world-stable.
    constexpr double kWaveWrap = 1024.0;
    const double waveAnchorX = std::floor(eyePosD.x / kWaveWrap) * kWaveWrap;
    const double waveAnchorZ = std::floor(eyePosD.z / kWaveWrap) * kWaveWrap;
    const glm::vec2 waveAnchor(static_cast<float>(eyePosD.x - waveAnchorX),
                               static_cast<float>(eyePosD.z - waveAnchorZ));
    shader.setVec2("waveAnchor", waveAnchor);
    shader.setFloat("waveTime", waveTime);

    // Both shaders sample skyLUT outside the fog path (ocean: distance color
    // shift; placed: sky reflection term), so bind it unconditionally.
    const GLuint skyLUTTex = lighting->getSkyLUTTexture();
    glActiveTexture(GL_TEXTURE0 + TextureUnits::SKY_LUT);
    glBindTexture(GL_TEXTURE_2D, skyLUTTex);
    shader.setInt("skyLUT", TextureUnits::SKY_LUT);
    shader.setFloat("skyExposure", lighting->getSkyExposure());

    uploadFogUniforms(shader, fogEnabled, skyLUTTex,
                      lighting->getSkyExposure(), fogStart, fogEnd, fogStrength,
                      lighting->getDirectionalLightDirection());

    glActiveTexture(GL_TEXTURE0 + TextureUnits::WATER_DUDV);
    glBindTexture(GL_TEXTURE_2D, dudvTexture);
    shader.setInt("dudvMap", TextureUnits::WATER_DUDV);

    glActiveTexture(GL_TEXTURE0 + TextureUnits::WATER_NORMAL);
    glBindTexture(GL_TEXTURE_2D, waterNormalTexture);
    shader.setInt("normalMap", TextureUnits::WATER_NORMAL);
}

void WaterRenderer::renderWaterSurface(const glm::mat4& projection) {
    prepareRender();

    const glm::mat4 view = camera->getViewMatrix();
    glm::mat4 viewRot = view;
    viewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    const glm::dvec3 eyePosD = camera->getEyePosD();
    const glm::vec3 sunDir = lighting->getDirectionalLightDirection();

    setupSurfaceShader(*waterShader, projection, viewRot, eyePosD, sunDir);

    // Ocean-only uniforms: depth linearization + FBO sampler binds.
    waterShader->setFloat("nearPlane", 0.1f);
    waterShader->setFloat("farPlane", 1000.0f);

    glActiveTexture(GL_TEXTURE0 + TextureUnits::WATER_REFLECT);
    glBindTexture(GL_TEXTURE_2D, fbos->getReflectionTexture());
    waterShader->setInt("reflectionTexture", TextureUnits::WATER_REFLECT);

    glActiveTexture(GL_TEXTURE0 + TextureUnits::WATER_REFRACT);
    glBindTexture(GL_TEXTURE_2D, fbos->getRefractionTexture());
    waterShader->setInt("refractionTexture", TextureUnits::WATER_REFRACT);

    glActiveTexture(GL_TEXTURE0 + TextureUnits::WATER_DEPTH);
    glBindTexture(GL_TEXTURE_2D, fbos->getRefractionDepthTexture());
    waterShader->setInt("refractionDepthTexture", TextureUnits::WATER_DEPTH);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    renderer->renderWater(*waterShader, eyePosD);
    glDisable(GL_BLEND);
}

void WaterRenderer::renderPlacedWaterSurface(const glm::mat4& projection) {
    if (!placedWaterShader) return;

    const glm::mat4 view = camera->getViewMatrix();
    glm::mat4 viewRot = view;
    viewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    const glm::dvec3 eyePosD = camera->getEyePosD();
    const glm::vec3 sunDir = lighting->getDirectionalLightDirection();

    setupSurfaceShader(*placedWaterShader, projection, viewRot, eyePosD, sunDir);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    renderer->renderPlacedWater(*placedWaterShader, eyePosD);
    glDisable(GL_BLEND);
}
