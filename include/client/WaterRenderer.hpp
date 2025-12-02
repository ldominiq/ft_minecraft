//
// Created by lucas on 10/14/25.
//

#ifndef WATERRENDERER_HPP
#define WATERRENDERER_HPP

#include <iostream>
#include <memory>
#include <glad/glad.h>
#include <glm/glm.hpp>

#include "Shader.hpp"
#include "WaterFramebuffer.hpp"

// Forward declarations
class Camera;
class Renderer;
class Lighting;

class WaterRenderer {
public:
    WaterRenderer(const std::shared_ptr<Shader>& shader, const std::shared_ptr<WaterFramebuffer>& fbos);
    ~WaterRenderer();
    
    // Set dependencies
    void setDependencies(const std::shared_ptr<Lighting>& lightingRef, const std::shared_ptr<Renderer>& rendererRef, const std::shared_ptr<Camera>& cameraRef,
                                     GLuint dudvTex, GLuint waterNormalTex);

    // Water rendering helper methods
    void renderWaterReflectionPass(const std::shared_ptr<Shader>& sceneShader, const glm::mat4& projection, unsigned int tex);
    void renderWaterRefractionPass(const std::shared_ptr<Shader>& sceneShader, const glm::mat4& view, const glm::mat4& projection, unsigned int tex);
    void renderWaterSurface(const glm::mat4& projection);

    float getWaterMoveFactor() const { return waterMoveFactor; }
    void setWaterMoveFactor(const float factor) { waterMoveFactor = factor; }
    float waveStrength = 0.02f;
    float dudvTiling = 0.03f;

private:
    std::shared_ptr<Shader> waterShader;
    std::shared_ptr<WaterFramebuffer> fbos;
    std::shared_ptr<Lighting> lighting;
    std::shared_ptr<Renderer> renderer;
    std::shared_ptr<Camera> camera;

    GLuint overlayVAO = 0, overlayVBO = 0;
    float waterMoveFactor = 0.0f;

    float seaLevel = 65.0f;
    GLuint dudvTexture = 0;
    GLuint waterNormalTexture = 0;
    GLuint texture = 0;  // Block texture atlas
    int screenWidth = 0;
    int screenHeight = 0;

    void prepareRender();
};


#endif //WATERRENDERER_HPP