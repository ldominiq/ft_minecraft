#ifndef LIGHTING_HPP
#define LIGHTING_HPP

#include <glm/gtc/type_ptr.hpp>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>
#include <memory>

#include "Shader.hpp"

static const float lightCubeVertices[] = {
    // positions only (36 vertices -> 12 triangles)

    // Front face (+Z)
    -0.5f, -0.5f,  0.5f,
    0.5f, -0.5f,  0.5f,
    0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,
    0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,

    // Back face (-Z)
    -0.5f, -0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f,
    0.5f,  0.5f, -0.5f,
    -0.5f, -0.5f, -0.5f,
    0.5f,  0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,

    // Left face (-X)
    -0.5f, -0.5f, -0.5f,
    -0.5f, -0.5f,  0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,
    -0.5f,  0.5f,  0.5f,
    -0.5f,  0.5f, -0.5f,

    // Right face (+X)
    0.5f, -0.5f, -0.5f,
    0.5f,  0.5f,  0.5f,
    0.5f, -0.5f,  0.5f,
    0.5f, -0.5f, -0.5f,
    0.5f,  0.5f, -0.5f,
    0.5f,  0.5f,  0.5f,

    // Bottom face (-Y)
    -0.5f, -0.5f, -0.5f,
    0.5f, -0.5f, -0.5f,
    0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f, -0.5f,
    0.5f, -0.5f,  0.5f,
    -0.5f, -0.5f,  0.5f,

    // Top face (+Y)
    -0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f,  0.5f,
    0.5f,  0.5f,  0.5f,
    0.5f,  0.5f,  0.5f,
    0.5f,  0.5f, -0.5f,
    -0.5f,  0.5f, -0.5f
};

class Lighting {
public:
    explicit Lighting(int screenWidth, int screenHeight);
    ~Lighting();

    void drawSky(const glm::mat4& view, const glm::mat4& projection, glm::vec3 cameraPos) const;
    void drawLightCubes(const glm::mat4& view, const glm::mat4& projection) const;

    void updateSunDirection(float deltaTime);

    // GETTERS
    inline glm::vec3 getDirectionalLightDirection() const { return directionalLightDir; }; 
    inline glm::vec3 getLightPos() const { return lightPos; };
    inline bool isDirectionalLightOn() const { return directionalLightOn; };
    inline glm::vec3 getDirectionalAmbientColor() const { return directionalAmbientColor; };
    inline glm::vec3 getDirectionalDiffuseColor() const { return directionalDiffuseColor; };
    inline glm::vec3 getDirectionalSpecularColor() const { return directionalSpecularColor; };
    inline float getMaterialShininess() const { return materialShininess; };
    inline bool isFlashlightOn() const { return flashlightOn; };
    inline float getFlashlightConstant() const { return spotLightConstant; };
    inline float getFlashlightLinear() const { return spotLightLinear; };
    inline float getFlashlightQuadratic() const { return spotLightQuadratic; };
    inline float getFlashlightCutoff() const { return flashlightCutoff; };
    inline float getFlashlightOuterCutoff() const { return flashlightOuterCutoff; };
    inline float getSkyExposure() const { return skyExposure; };
    inline float getSkyAtmDensity() const { return skyAtmDensity; };
    inline float getSkyAtmThickness() const { return skyAtmThickness; };
    inline float getPlanetScale() const { return planetScale; };
    inline bool isSkyTimePaused() const { return skyTimePaused; };
    inline float getSkyTimeOffset() const { return skyTimeOffset; };
    inline bool isSpotLightOn() const { return flashlightOn; };
    inline float getSpotLightConstant() const { return spotLightConstant; };
    inline float getSpotLightLinear() const { return spotLightLinear; };
    inline float getSpotLightQuadratic() const { return spotLightQuadratic; };
    inline float getFlashlightCutoffAngle() const { return flashlightCutoff; };
    inline float getFlashlightOuterCutoffAngle() const { return flashlightOuterCutoff; };
    
    bool isPointLightOn(int index) const;
    glm::vec3 getPointLightPosition(int index) const;
    glm::vec3 getPointLightAmbient(int index) const;
    glm::vec3 getPointLightDiffuse(int index) const;
    glm::vec3 getPointLightSpecular(int index) const;
    float getPointLightConstant(int index) const;
    float getPointLightLinear(int index) const;
    float getPointLightQuadratic(int index) const;
    

    // SETTERS
    inline void setLightPos(const glm::vec3& pos) { lightPos = pos; };
    inline void setScreenDimensions(int screenWidth, int screenHeight) { width = screenWidth; height = screenHeight; };

private:
    unsigned int skyVAO;
    unsigned int skyVBO;
    unsigned int lightCubeVAO, lightCubeVBO;
    std::unique_ptr<Shader> skyShader;
    std::unique_ptr<Shader> lightCubeShader;

    // Screen dimensions for sky shader
    int width;
    int height;

    // Directional light (sun)
    bool directionalLightOn = true;
    glm::vec3 directionalLightDir  = glm::vec3(0.5f, 1.0f, 0.3f);
    glm::vec3 lightPos = directionalLightDir * 200.0f;
    glm::vec3 directionalAmbientColor = glm::vec3(0.3f);
    glm::vec3 directionalDiffuseColor = glm::vec3(1.0f);
    glm::vec3 directionalSpecularColor = glm::vec3(1.0f);
    float sunYawDeg = 45.0f;   // horizontal rotation of the sun path (0 = along +X, 90 = along +Z)

    // --- Sky controls ---
    // Control sun position over time
    float deltaTime;
    float skyTimeOffset = 0.0f;
    bool skyTimePaused = false;
    // Simple tone-mapping exposure for sky shader
    float skyExposure = 1.2f;
    // Atmospheric density and thickness scalars (1.0 ~ Earth-like)
    float skyAtmDensity = 19.0f;
    float skyAtmThickness = 1.0f;
    float planetScale = 7900.0f;

    // Point light (lamp)
    std::vector<bool> pointLightsOn = {true, true, true};
    glm::vec3 pointLightPositions[3] = {
        glm::vec3( 0.0f, 90.0f, 0.0f),
        glm::vec3( 4.0f, 90.0f, 0.0f),
        glm::vec3( 8.0f, 90.0f, 0.0f)
    };
    glm::vec3 pointLightAmbient[3] = {
        glm::vec3(1.0, 0.0, 0.0),
        glm::vec3(0.0, 1.0, 0.0),
        glm::vec3(0.0, 0.0, 1.0)
    };
    glm::vec3 pointLightDiffuse[3] = {
        glm::vec3(1.0, 0.0, 0.0),
        glm::vec3(0.0, 1.0, 0.0),
        glm::vec3(0.0, 0.0, 1.0)
    };
    glm::vec3 pointLightSpecular[3] = {
        glm::vec3(1.0, 0.0, 0.0),
        glm::vec3(0.0, 1.0, 0.0),
        glm::vec3(0.0, 0.0, 1.0)
    };
    float pointLightConstant[3] = { 1.0f, 1.0f, 1.0f };
    float pointLightLinear[3] = { 0.09f, 0.09f, 0.09f };
    float pointLightQuadratic[3] = { 0.032f, 0.032f, 0.032f };

    // Flashlight
    bool flashlightOn = false;
    float spotLightConstant = 1.0f;
    float spotLightLinear = 0.09f;
    float spotLightQuadratic = 0.032f;
    float flashlightCutoff = 12.5f; // spotlight cutoff angle in degrees
    float flashlightOuterCutoff = 17.5f; // spotlight outer cutoff angle in degrees

    float materialShininess = 32.0f; // material shininess factor
};

#endif