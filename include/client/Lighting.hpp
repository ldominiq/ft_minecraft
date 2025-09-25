#ifndef LIGHTING_HPP
#define LIGHTING_HPP

#include <glm/gtc/type_ptr.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

#include "Shader.hpp"
#include "Renderer.hpp"

static constexpr float lightCubeVertices[] = {
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

static constexpr float planeVertices[] = {
    // positions            // normals         // texcoords
    25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
   -25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,   0.0f,  0.0f,
   -25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,

    25.0f, -0.5f,  25.0f,  0.0f, 1.0f, 0.0f,  25.0f,  0.0f,
   -25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,   0.0f, 25.0f,
    25.0f, -0.5f, -25.0f,  0.0f, 1.0f, 0.0f,  25.0f, 25.0f
};

class Lighting {
public:
    explicit Lighting(int screenWidth, int screenHeight);
    ~Lighting();

    void drawSky(const glm::mat4& view, const glm::mat4& projection, glm::vec3 cameraPos) const;
    void drawLightCubes(const glm::mat4& view, const glm::mat4& projection) const;

    void updateSunDirection(float deltaTime);

    void uploadLightingUniforms(const Shader& shader, const glm::vec3& cameraPos, glm::vec3 cameraFront) const;

    void updateShadowMap(const Renderer& renderer, const glm::vec3& cameraPos);
    void refreshShadowResolution();

    void initShadowGroundPlane();
    void initShadowResources();
    void drawShadowMapPreview() const;
    void initShadowDebugShader() const;

    static void drawTexturePreviewQuad(unsigned int textureID);

    enum class ShadowQuality {
        Low = 1024,
        Medium = 2048,
        High = 4096,
        Ultra = 8192
    };

    // GETTERS
    bool isDirectionalLightOn() const { return directionalLightOn; };
    bool isFlashlightOn() const { return flashlightOn; };
    bool isSkyTimePaused() const { return skyTimePaused; };
    bool isSpotLightOn() const { return flashlightOn; };
    bool isShadowsEnabled() const { return shadowsEnabled; };

    glm::vec3 getDirectionalLightDirection() const { return directionalLightDir; };
    glm::vec3 getLightPos() const { return lightPos; };
    glm::vec3 getDirectionalAmbientColor() const { return directionalAmbientColor; };
    glm::vec3 getDirectionalDiffuseColor() const { return directionalDiffuseColor; };
    glm::vec3 getDirectionalSpecularColor() const { return directionalSpecularColor; };

    int getShadowUpdateInterval() const { return shadowUpdateInterval; };
    int getShadowFrameCounter() const { return shadowFrameCounter; };
    ShadowQuality getShadowQuality() const { return shadowQuality; };
    float getShadowOrthoRange() const { return shadowOrthoRange; };
    float getShadowNearPlane() const { return shadowNearPlane; };
    float getShadowFarPlane() const { return shadowFarPlane; };
    float getMaterialShininess() const { return materialShininess; };

    float getSkyExposure() const { return skyExposure; };
    float getSkyAtmDensity() const { return skyAtmDensity; };
    float getSkyAtmThickness() const { return skyAtmThickness; };
    float getSkyTimeOffset() const { return skyTimeOffset; };
    float getPlanetScale() const { return planetScale; };
    float getSunYawDeg() const { return sunYawDeg; };

    float getSpotLightConstant() const { return spotLightConstant; };
    float getSpotLightLinear() const { return spotLightLinear; };
    float getSpotLightQuadratic() const { return spotLightQuadratic; };
    float getFlashlightCutoffAngle() const { return flashlightCutoff; };
    float getFlashlightOuterCutoffAngle() const { return flashlightOuterCutoff; };

    size_t getNumPointLights() const { return pointLightsOn.size(); };

    bool isPointLightOn(int index) const;
    glm::vec3 getPointLightPosition(int index) const;
    glm::vec3 getPointLightAmbient(int index) const;
    glm::vec3 getPointLightDiffuse(int index) const;
    glm::vec3 getPointLightSpecular(int index) const;
    float getPointLightConstant(int index) const;
    float getPointLightLinear(int index) const;
    float getPointLightQuadratic(int index) const;
    

    // SETTERS
    void setLightPos(const glm::vec3& pos) { lightPos = pos; };
    void setViewportSize(const int screenWidth, const int screenHeight) { width = screenWidth; height = screenHeight; };

    void setShadowsEnabled(const bool enabled) { shadowsEnabled = enabled; };
    void setShadowQuality(const ShadowQuality quality) { shadowQuality = quality; };
    void setShadowUpdateInterval(const int interval) { shadowUpdateInterval = interval; };
    void setShadowOrthoRange(const float range) { shadowOrthoRange = range; };
    void setShadowNearPlane(const float nearPlane) { shadowNearPlane = nearPlane; };
    void setShadowFarPlane(const float farPlane) { shadowFarPlane = farPlane; };

    void setMaterialShininess(const float shininess) { materialShininess = shininess; };

    void setSpotLightOn(const bool enabled) { flashlightOn = enabled; };
    void setSpotLightConstant(const float constant) { spotLightConstant = constant; };
    void setSpotLightLinear(const float linear) { spotLightLinear = linear; };
    void setSpotLightQuadratic(const float quadratic) { spotLightQuadratic = quadratic; };
    void setFlashlightCutoffAngle(const float cutoff) { flashlightCutoff = cutoff; };
    void setFlashlightOuterCutoffAngle(const float outerCutoff) { flashlightOuterCutoff = outerCutoff; };

    void setDirectionalLightEnabled(const bool enabled) { directionalLightOn = enabled; };
    void setDirectionalLightDirection(const glm::vec3& dir) { directionalLightDir = dir; };
    void setDirectionalAmbientColor(const glm::vec3& color) { directionalAmbientColor = color; };
    void setDirectionalDiffuseColor(const glm::vec3& color) { directionalDiffuseColor = color; };
    void setDirectionalSpecularColor(const glm::vec3& color) { directionalSpecularColor = color; };

    void setSkyExposure(const float exposure) { skyExposure = exposure; };
    void setSkyAtmDensity(const float density) { skyAtmDensity = density; };
    void setSkyAtmThickness(const float thickness) { skyAtmThickness = thickness; };
    void setSkyTimeOffset(const float offset) { skyTimeOffset = offset; };
    void setSkyTimePaused(const bool paused) { skyTimePaused = paused; };
    void setSunYawDeg(const float yawDeg) { sunYawDeg = yawDeg; };
    void setPlanetScale(const float scale) { planetScale = scale; };

    void setPointLightEnabled(int index, bool enabled);
    void setPointLightPosition(int index, const glm::vec3& pos);
    void setPointLightAmbient(int index, const glm::vec3& color);
    void setPointLightDiffuse(int index, const glm::vec3& color);
    void setPointLightSpecular(int index, const glm::vec3& color);
    void setPointLightConstant(int index, float constant);
    void setPointLightLinear(int index, float linear);
    void setPointLightQuadratic(int index, float quadratic);

private:
    unsigned int skyVAO{};
    unsigned int lightCubeVAO{}, lightCubeVBO{};
    unsigned int planeVAO{};
    std::unique_ptr<Shader> skyShader;
    std::unique_ptr<Shader> lightCubeShader;
    std::shared_ptr<Shader> shadowDepthShader;
    std::shared_ptr<Shader> shadowDebugShader;

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

    // SHADOWS
    glm::mat4 lightProjection{}, lightView{};
    glm::mat4 lightSpaceMatrix {1.0f};
    glm::vec3 cachedShadowLightDir {0.0f, -1.0f, 0.0f};
    int shadowFrameCounter = 0;
    int shadowUpdateInterval = 4;

    bool forceShadowUpdate = false;
    bool shadowsEnabled = true;
    float shadowOrthoRange = 200.0f;

    unsigned int depthMapFBO{}, depthMap{};


    ShadowQuality shadowQuality = ShadowQuality::High;
    int SHADOW_WIDTH = static_cast<int>(shadowQuality);
    int SHADOW_HEIGHT = static_cast<int>(shadowQuality);

    // Default shadow map near/far plane values
    float shadowNearPlane = 0.1f;
    float shadowFarPlane = 400.0f;

    int PCF_RADIUS = 1;          // 1 = 3x3;
    float MIN_BIAS = 0.00035;
    float MAX_BIAS = 0.0010;
    float shadowContactOffset = 0.00050f;

    int   POISSON_SAMPLES = 16;
    float POISSON_RADIUS_BASE = 1.75;   // start radius in texels
    float POISSON_RADIUS_SCALE = 1.0;   // extra scale factor
};

#endif