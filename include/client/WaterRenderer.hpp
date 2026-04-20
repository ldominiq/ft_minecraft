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
    WaterRenderer(const std::shared_ptr<Shader>& shader, const std::shared_ptr<WaterFramebuffer>& fbos);
    ~WaterRenderer();
    
    // Set dependencies
    void setDependencies(const std::shared_ptr<Lighting>& lightingRef, const std::shared_ptr<Renderer>& rendererRef, const std::shared_ptr<Camera>& cameraRef);

    // Water rendering helper methods
    void renderWaterReflectionPass(const std::shared_ptr<Shader>& sceneShader, const glm::mat4& projection, const TextureManager& texMgr);
    void renderWaterRefractionPass(const std::shared_ptr<Shader>& sceneShader, const glm::mat4& view, const glm::mat4& projection, const TextureManager& texMgr);
    void renderWaterSurface(const glm::mat4& projection);

    float getWaterMoveFactor() const { return waterMoveFactor; }
    void setWaterMoveFactor(const float factor) { waterMoveFactor = factor; }
    void setFogParams(bool enabled, float start, float end, float strength = 1.4f) {
        fogEnabled = enabled;
        fogStart = start;
        fogEnd = end;
        fogStrength = strength;
    }
    float waveStrength = 0.02f;
    float dudvTiling = 0.03f;

private:
    std::shared_ptr<Shader> waterShader;
    std::shared_ptr<WaterFramebuffer> fbos;
    std::shared_ptr<Lighting> lighting;
    std::shared_ptr<Renderer> renderer;
    std::shared_ptr<Camera> camera;

    float waterMoveFactor = 0.0f;

    bool  fogEnabled  = false;
    float fogStart    = 500.0f;
    float fogEnd      = 950.0f;
    float fogStrength = 1.4f;

    float seaLevel = 65.0f;
    GLuint dudvTexture = 0;
    GLuint waterNormalTexture = 0;

    void prepareRender();
};


#endif //WATERRENDERER_HPP