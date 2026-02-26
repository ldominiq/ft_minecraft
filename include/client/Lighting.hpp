#ifndef LIGHTING_HPP
#define LIGHTING_HPP

#include <glm/gtc/type_ptr.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <memory>

#include "Shader.hpp"
#include "Renderer.hpp"
#include "CloudFramebuffer.hpp"

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

    void drawTexturePreviewQuad(unsigned int textureID);

    void renderCloudsLowRes(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos) const;

    // CSM
    static std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);
    glm::mat4 getLightSpaceMatrix(const float nearPlane, const float farPlane, const glm::mat4& view) const;
    std::vector<glm::mat4> getLightSpaceMatrices(const glm::mat4& cameraView) const;
    void initCSMResources();
    void updateCSMShadowMaps(const Renderer& renderer, const glm::mat4& cameraView);
    void uploadCSMUniforms(const Shader& shader, const glm::mat4& cameraView) const;
    void drawCSMShadowMapPreview(int cascadeLayer);
    void drawCSMDebugView(const glm::vec3& cameraPos, const glm::vec3& cameraFront, const glm::mat4& cameraView);
    bool debugCascades = false;
    bool showCSMDebugView = false;
    std::vector<float> shadowCascadeLevels{ 25.0f, 100.0f };  // 2 splits → 3 cascades: [0.1–25], [25–100], [100–500]
    int debugPreviewLayer = 0;


    // GETTERS
    bool isDirectionalLightOn() const { return directionalLightOn; };
    bool isFlashlightOn() const { return flashlightOn; };
    bool isSkyTimePaused() const { return skyTimePaused; };
    bool isSpotLightOn() const { return flashlightOn; };
    bool isShadowsEnabled() const { return shadowsEnabled; };
    bool isShadowMapEnabled() const { return showShadowMap; };
    bool isCloudsEnabled() const { return cloudsEnabled; };
    bool isSunAboveHorizon() const { return directionalLightDir.y > 0.1f; }
    
    GLuint getCloudTexture() const;

    glm::vec3 getDirectionalLightDirection() const { return directionalLightDir; };
    glm::vec3 getLightPos() const { return lightPos; };
    glm::vec3 getDirectionalAmbientColor() const { return directionalAmbientColor; };
    glm::vec3 getDirectionalDiffuseColor() const { return directionalDiffuseColor; };
    glm::vec3 getDirectionalSpecularColor() const { return directionalSpecularColor; };

    float getMaterialShininess() const { return materialShininess; };
    float getShadowMapMinBias() const { return MIN_BIAS; }
    float getShadowMapMaxBias() const { return MAX_BIAS; }

    float getSkyExposure() const { return skyExposure; };
    float getSkyAtmDensity() const { return skyAtmDensity; };
    float getSkyAtmThickness() const { return skyAtmThickness; };
    float getSkyTimeOffset() const { return skyTimeOffset; };
    float getPlanetScale() const { return planetScale; };
    float getSunYawDeg() const { return sunYawDeg; };

    float getCloudDensity() const { return cloudDensity; };
    float getCloudSigmaT() const { return cloudSigmaT; };
    glm::vec3 getCloudAlbedo() const { return cloudAlbedo; };
    float getCloudStepCount() const { return cloudStepCount; };
    float getCloudSigmaS() const { return cloudSigmaS; };
    float getCloudPhaseG() const { return cloudPhaseG; };
    float getCloudEdgeFeather() const { return cloudEdgeFeather; };
    float getCloudNoiseScale() const { return cloudNoiseScale; };
    float getCloudNoiseContrastLo() const { return cloudNoiseContrastLo; };
    float getCloudNoiseContrastHi() const { return cloudNoiseContrastHi; };
    float getCloudWindSpeed() const { return cloudWindSpeed; };
    glm::vec2 getCloudWindDir() const { return cloudWindDir; };


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
    void setShowShadowMapEnabled(bool enabled) { showShadowMap = enabled; };
    void setShadowMapMinBias(float bias) { MIN_BIAS = bias; };
    void setShadowMapMaxBias(float bias) { MAX_BIAS = bias; };
    void setLightPos(const glm::vec3& pos) { lightPos = pos; };
    void setViewportSize(const int screenWidth, const int screenHeight) { width = screenWidth; height = screenHeight; };

    void setShadowsEnabled(const bool enabled) { shadowsEnabled = enabled; };

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
    void setCloudsEnabled(const bool enabled) { cloudsEnabled = enabled; };
    void setCloudDensity(const float density) { cloudDensity = density; };
    void setCloudSigmaT(const float sigmaT) { cloudSigmaT = sigmaT; };
    void setCloudAlbedo(const glm::vec3& albedo) { cloudAlbedo = albedo; };
    void setCloudStepCount(const float stepCount) { cloudStepCount = stepCount; };
    void setCloudSigmaS(const float sigmaS) { cloudSigmaS = sigmaS; };
    void setCloudPhaseG(const float phaseG) { cloudPhaseG = phaseG; };
    void setCloudEdgeFeather(const float feather) { cloudEdgeFeather = feather; };
    void setCloudNoiseScale(const float scale) { cloudNoiseScale = scale; };
    void setCloudNoiseContrastLo(const float lo) { cloudNoiseContrastLo = lo; };
    void setCloudNoiseContrastHi(const float hi) { cloudNoiseContrastHi = hi; };
    void setCloudWindSpeed(const float speed) { cloudWindSpeed = speed; };
    void setCloudWindDir(const glm::vec2& dir) { cloudWindDir = dir; };

    void setPointLightEnabled(int index, bool enabled);
    void setPointLightPosition(int index, const glm::vec3& pos);
    void setPointLightAmbient(int index, const glm::vec3& color);
    void setPointLightDiffuse(int index, const glm::vec3& color);
    void setPointLightSpecular(int index, const glm::vec3& color);
    void setPointLightConstant(int index, float constant);
    void setPointLightLinear(int index, float linear);
    void setPointLightQuadratic(int index, float quadratic);

private:
    GLuint skyVAO{};
    GLuint lightCubeVAO{}, lightCubeVBO{};
    GLuint planeVAO{};
    GLuint debugVAO{};
    GLuint debugVBO{};
    GLuint cloudsVAO{};

    std::unique_ptr<CloudFramebuffer> cloudFBO;

    std::unique_ptr<Shader> skyShader;
    std::unique_ptr<Shader> lightCubeShader;
    std::shared_ptr<Shader> debugFBOShader;
    std::shared_ptr<Shader> cloudShader;
    
    // CSM
    std::shared_ptr<Shader> csmDepthShader;
    GLuint csmFBO = 0;
    GLuint csmDepthMaps = 0;
    unsigned int depthMapResolution = 2048;
    float cameraFarPlane = 500.0f;
    std::vector<glm::mat4> csmLightSpaceMatrices;

    // Screen dimensions
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

    // Cloud controls
    bool cloudsEnabled = true;
    float cloudDensity = 0.08f; // overall cloud density (increased for more visible clouds)
    float cloudSigmaT = 2.0f; // extinction coefficient (lower = less absorption, brighter clouds)
    glm::vec3 cloudAlbedo = glm::vec3(1.0f); // cloud albedo (reflectivity)
    float cloudStepCount = 48.0f; // number of steps (lower for performance, still good quality)

    float cloudSigmaS = 2.0f; // scattering coefficient
    float cloudPhaseG = 0.4f; // phase function (lower = more uniform scattering, less directional)

    int cloudDownscale = 4; // downscaling factor for cloud rendering (higher = faster but blurrier)

    float cloudEdgeFeather = 8.0f;      // smaller feather = sharper edges
    float cloudNoiseScale = 0.005f;     // lower frequency = bigger, chunkier clouds
    float cloudNoiseContrastLo = 0.58f; // tighter contrast range for more defined shapes
    float cloudNoiseContrastHi = 1.0f;
    float cloudWindSpeed = 100.0f;
    glm::vec2 cloudWindDir = glm::vec2(1.0f, 0.2f); // mostly horizontal drift
    

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

    bool forceShadowUpdate = false;
    bool shadowsEnabled = true;

    float MIN_BIAS = 0.001;
    float MAX_BIAS = 0.005;

    // DEBUG
    bool showShadowMap = false;
};

#endif