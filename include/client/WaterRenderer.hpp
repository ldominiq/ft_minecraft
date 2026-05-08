//
// Created by lucas on 10/14/25.
//

#ifndef WATERRENDERER_HPP
#define WATERRENDERER_HPP

#include <memory>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Shader.hpp"
#include "WaterFramebuffer.hpp"

// Forward declarations
class Camera;
class Renderer;
class Lighting;
class TextureManager;

class WaterRenderer {
public:
    WaterRenderer(int screenWidth, int screenHeight);
    ~WaterRenderer();
    
    // Set dependencies
    void setDependencies(const std::shared_ptr<Lighting>& lightingRef, const std::shared_ptr<Renderer>& rendererRef, const std::shared_ptr<Camera>& cameraRef);

    // Water rendering helper methods
    void renderWaterReflectionPass(const std::shared_ptr<Shader>& sceneShader, const glm::mat4& projection, const TextureManager& texMgr);
    void renderWaterRefractionPass(const std::shared_ptr<Shader>& sceneShader, const glm::mat4& view, const glm::mat4& projection, const TextureManager& texMgr);
    void renderWaterSurface(const glm::mat4& projection);

    // Sky-reflection pass for water that isn't on the sea-level plane. Cheap;
    // shares the dudv/normal map and skyLUT with the ocean pass but binds no
    // reflection/refraction FBO textures.
    void renderPlacedWaterSurface(const glm::mat4& projection);

    float getWaterMoveFactor() const { return waterMoveFactor; }
    void setWaterMoveFactor(const float factor) { waterMoveFactor = factor; }

    
    float getWaterMoveFactor2() const { return waterMoveFactor2; }
    void  setWaterMoveFactor2(const float factor) { waterMoveFactor2 = factor; }

    // Non-wrapping wave time used for Gerstner displacement. moveFactor wraps
    // 0->1 every second (which is fine for additive UV scrolling on a tiling
    // dudv texture) but would cause a visible phase snap if fed into cos/sin.
    float getWaveTime() const { return waveTime; }
    void  advanceWaveTime(float dt);
    void setFogParams(bool enabled, float start, float end, float strength = 1.4f) {
        fogEnabled = enabled;
        fogStart = start;
        fogEnd = end;
        fogStrength = strength;
    }
    float waveStrength = 0.02f;
    float dudvTiling = 0.03f;

    // ── Runtime graphics-quality settings ──────────────────────────────
    // Toggling these is free at steady state — they're either uniform/CPU
    // checks or one-shot FBO rebuilds (refraction scale).
    bool isReflectionEnabled() const           { return reflectionEnabled; }
    void setReflectionEnabled(bool enabled)    { reflectionEnabled = enabled; }

    bool isRefractionVegetationEnabled() const         { return refractionRendersVegetation; }
    void setRefractionVegetationEnabled(bool enabled)  { refractionRendersVegetation = enabled; }

    float getRefractionResolutionScale() const { return refractionResolutionScale; }
    void  setRefractionResolutionScale(float scale, int displayWidth, int displayHeight);

    float getReflectionMaxDistance() const     { return reflectionMaxDistance; }
    void  setReflectionMaxDistance(float d)    { reflectionMaxDistance = d; }

    int  getSeaLevel() const                   { return static_cast<int>(seaLevel); }
    void setSeaLevel(int sl)                   { seaLevel = static_cast<float>(sl); }

    // FBO texture accessors for debug GUI overlays.
    GLuint getReflectionTexture()      const { return fbos->getReflectionTexture(); }
    GLuint getRefractionTexture()      const { return fbos->getRefractionTexture(); }
    GLuint getRefractionDepthTexture() const { return fbos->getRefractionDepthTexture(); }

private:
    std::unique_ptr<Shader> waterShader;
    std::unique_ptr<Shader> placedWaterShader;
    std::unique_ptr<WaterFramebuffer> fbos;
    std::shared_ptr<Lighting> lighting;
    std::shared_ptr<Renderer> renderer;
    std::shared_ptr<Camera> camera;

    float waterMoveFactor = 0.0f;
    float waterMoveFactor2 = 0.0f;
    // Continuously-accumulating phase for Gerstner waves.
    float waveTime = 0.0f;

    bool  fogEnabled  = false;
    float fogStart    = 500.0f;
    float fogEnd      = 950.0f;
    float fogStrength = 1.4f;

    // Water *surface* Y (= TerrainGenerationParams::seaLevel + 1). The
    // terrain generator fills water blocks up to and including its
    // `seaLevel` index, so a block at y=64 has its top face at y=65. The
    // planar mirror and refraction clip plane both align to that surface.
    float seaLevel = 65.0f;
    GLuint dudvTexture = 0;
    GLuint waterNormalTexture = 0;

    // Runtime graphics-quality state.
    bool  reflectionEnabled = true;
    bool  refractionRendersVegetation = true;
    float refractionResolutionScale = 1.0f;  // 1.0 = full screen, 0.5 = half-res
    float reflectionMaxDistance = 0.0f;      // 0 = no cap (use main render distance)

    void prepareRender();
};


#endif //WATERRENDERER_HPP