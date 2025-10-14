//
// Created by lucas on 10/14/25.
//

#ifndef WATERRENDERER_HPP
#define WATERRENDERER_HPP

#include <iostream>
#include <memory>
#include <glad/glad.h>
#include <glm/glm.hpp>

class WaterRenderer {
public:
    WaterRenderer();
    ~WaterRenderer();

    // Water rendering helper methods
    void renderWaterReflectionPass(const glm::mat4& projection, float seaLevel);
    void renderWaterRefractionPass(const glm::mat4& view, const glm::mat4& projection);
    void renderWaterSurface(const glm::mat4& view, const glm::mat4& projection, float seaLevel);
    void renderUnderWater();
    glm::mat4 calculateReflectedViewMatrix(float seaLevel) const;

    float getWaterMoveFactor() const { return waterMoveFactor; }
    void setWaterMoveFactor(const float factor) { waterMoveFactor = factor; }

private:

    GLuint overlayVAO = 0, overlayVBO = 0;
    float waterMoveFactor = 0.0f;
};


#endif //WATERRENDERER_HPP