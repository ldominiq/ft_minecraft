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
    WaterRenderer(const std::shared_ptr<Shader>& shader, const std::shared_ptr<WaterFramebuffer> &fbos);
    ~WaterRenderer();
    
    // Set dependencies
    void setDependencies(
                        GLuint dudvTex);

    // Water rendering helper methods
    void renderWaterReflectionPass(const std::shared_ptr<Lighting> &lighting, const std::shared_ptr<Renderer> &renderer, const std::shared_ptr<Shader>& sceneShader, const std::shared_ptr<Camera>& camera, const glm::mat4& projection, float seaLevel, unsigned int tex);
    void renderWaterRefractionPass(const std::shared_ptr<Lighting> &lighting, const std::shared_ptr<Camera> &camera, const std::shared_ptr<Renderer> &renderer, const std::shared_ptr<Shader>& sceneShader, const glm::mat4& view, const glm::mat4& projection, float seaLevel, unsigned int tex);
    void renderWaterSurface(const std::shared_ptr<Renderer> &renderer, const std::shared_ptr<Camera> &camera, const glm::mat4& projection, float seaLevel);
    void renderUnderWater();
    glm::mat4 calculateReflectedViewMatrix(const Camera& camera, float seaLevel) const;

    float getWaterMoveFactor() const { return waterMoveFactor; }
    void setWaterMoveFactor(const float factor) { waterMoveFactor = factor; }
    float waveStrength = 0.02f;
    float dudvTiling = 0.03f;

private:
    std::shared_ptr<Shader> waterShader;
    std::shared_ptr<WaterFramebuffer> fbos;
    GLuint overlayVAO = 0, overlayVBO = 0;
    float waterMoveFactor = 0.0f;

    GLuint dudvTexture = 0;
    GLuint waterNormalTexture = 0;
    GLuint texture = 0;  // Block texture atlas
    std::shared_ptr<Shader> underwaterOverlayShader;
    int screenWidth = 0;
    int screenHeight = 0;
    float renderDistance = 0.0f;

    void prepareRender();
};


#endif //WATERRENDERER_HPP