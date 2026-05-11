#ifndef LIGHTING_HPP
#define LIGHTING_HPP

#include <glm/gtc/type_ptr.hpp>
#include <glm/glm.hpp>
#include <vector>
#include <memory>
#include <algorithm>
#include <cmath>
#include <imgui.h>
#include <limits>
#include <cstdio>

#include "Shader.hpp"
#include "Renderer.hpp"
#include "CloudFramebuffer.hpp"
#include "SkyLUT.hpp"

class TextureManager;

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

    // destIsHDR: if true, the bound framebuffer is HDR (RGBA16F) and the sky
    // shader will skip its own tonemap (clouds_composite tonemaps later).
    // Water reflections render into an LDR FBO, so they pass destIsHDR=false.
    void drawSky(const glm::mat4& view, const glm::mat4& projection, glm::vec3 cameraPos, bool cameraUnderwater = false, bool destIsHDR = true) const;
    void drawLightCubes(const glm::mat4& view, const glm::mat4& projection, const glm::dvec3& eyePos) const;

    void updateSunDirection(float deltaTime);
    /// Update the sky scattering LUT (call once per frame, before drawSky).
    /// Only regenerates when atmosphere parameters or camera height change.
    void updateSkyLUT(float cameraPosY);

    void uploadLightingUniforms(const Shader& shader, const glm::dvec3& eyePos, glm::vec3 cameraFront) const;
    void uploadUnderwaterUniforms(const Shader& shader) const;
    void drawTexturePreviewQuad(unsigned int textureID, bool grayscale = false, glm::vec2 offset = glm::vec2(0.0f));

    void renderCloudsLowRes(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos) const;

    // Post-terrain pass: samples (sceneColor, sceneDepth, cloudTex) and composites
    // clouds over the scene using per-pixel depth comparison. Draws into the currently
    // bound framebuffer (FBO=0 in the typical case).
    void compositeCloudsToBackbuffer(GLuint sceneColorTex, GLuint sceneDepthTex,
                                     const glm::mat4& view, const glm::mat4& projection,
                                     const glm::vec3& cameraPosWorld,
                                     const glm::vec2& resolution) const;

    // CSM
    static std::vector<glm::vec4> getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view);
    glm::mat4 getLightSpaceMatrix(const float nearPlane, const float farPlane, const glm::mat4& view) const;
    std::vector<glm::mat4> getLightSpaceMatrices(const glm::mat4& cameraView) const;
    void initCSMResources();
    void updateCSMShadowMaps(const Renderer& renderer, const glm::mat4& cameraView,
                             const glm::dvec3& eyePos, const TextureManager& texMgr);
    void uploadCSMUniforms(const Shader& shader, const glm::mat4& cameraView) const;
    void drawCSMShadowMapPreview(int cascadeLayer);
    void drawCSMDebugView(const glm::vec3& cameraPos, const glm::vec3& cameraFront, const glm::mat4& cameraView);
    bool debugCascades = false;
    bool showCSMDebugView = false;
    std::vector<float> shadowCascadeLevels{ 40.0f };
    int debugPreviewLayer = 0;

    enum class PcfQuality : int { Low = 0, Medium = 1, High = 2 }; // 1-tap, 3x3, 5x5

    void setShadowMapResolution(unsigned int res); // triggers rebuildCSMResources()
    void setCascadeCount(int count); // 2 or 3 - triggers rebuild
    void setShadowFarPlane(float farPlane);
	void setPcfQuality(PcfQuality q) { pcfQuality = q; };
	void setShadowAlphaTest(bool enabled) { shadowAlphaTest = enabled; };

	unsigned int getShadowMapResolution() const { return depthMapResolution; };
    int getCascadeCount() const { return static_cast<int>(shadowCascadeLevels.size()) + 1; }
    float getShadowFarPlane() const { return cameraFarPlane; }
	PcfQuality getPcfQuality() const { return pcfQuality; }
	bool getShadowAlphaTest() const { return shadowAlphaTest; };

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
    GLuint getSkyLUTTexture() const { return (skyLUT ? skyLUT->getLUTTexture() : 0); }

    glm::vec3 getDirectionalLightDirection() const { return directionalLightDir; };
    glm::vec3 getLightPos() const { return lightPos; };
    glm::vec3 getDirectionalAmbientColor() const { return directionalAmbientColor; };
    glm::vec3 getDirectionalDiffuseColor() const { return directionalDiffuseColor; };
    glm::vec3 getDirectionalSpecularColor() const { return directionalSpecularColor; };

    float getMaterialShininess() const { return materialShininess; };
    float getShadowMapMinBias() const { return MIN_BIAS; }
    float getShadowMapMaxBias() const { return MAX_BIAS; }

    float getSkyExposure() const { return skyExposure; };
    bool  isHDREnabled() const { return hdrEnabled; }
    void  setHDREnabled(bool v) { hdrEnabled = v; }
    float getSkySaturation() const { return skySaturation; }
    void  setSkySaturation(float v) { skySaturation = v; }
    float getSkyAtmDensity() const { return skyAtmDensity; };
    float getSkyAtmThickness() const { return skyAtmThickness; };
    float getSkyTimeOffset() const { return skyTimeOffset; };
    float getPlanetScale() const { return planetScale; };
    float getSunYawDeg() const { return sunYawDeg; };
    float getSkyTimeSpeed() const { return skyTimeSpeed; };
    float getSunPauseTimer() const { return sunPauseTimer; };
    float getSunStepTimer() const { return sunStepTimer; };
    bool getSunStepping() const { return sunStepping; };
    uint8_t getSkyMode() const { return skyMode; };

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

    glm::vec3 getUnderwaterTintColor() const { return underwaterTintColor; };
    glm::vec3 getUnderwaterFogColor() const { return underwaterFogColor; };
    float getUnderwaterFogDensity() const { return underwaterFogDensity; };


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
    void setSkyMode(const uint8_t mode) { skyMode = mode; };
    void setSkyTimeSpeed(const float speed) { skyTimeSpeed = speed; };
    void setSunStepping(const bool stepping) { sunStepping = stepping; };
    void setSunPauseTimer(const float t) { sunPauseTimer = t; };
    void setSunStepTimer(const float t) { sunStepTimer = t; };
    void setPlanetScale(const float scale) { planetScale = scale; };
    void setSkyLUTEnabled(bool enabled) { skyLUTEnabled = enabled; if (skyLUT) skyLUT->invalidate(); }
    bool isSkyLUTEnabled() const { return skyLUTEnabled; }
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

    void setUnderwaterTintColor(const glm::vec3 &tint) { underwaterTintColor = tint; };
    void setUnderwaterFogColor(const glm::vec3 &color) { underwaterFogColor = color; };
    void setUnderwaterFogDensity(const float density) { underwaterFogDensity = density; };

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
    std::unique_ptr<Shader> skyLUTRenderShader;  // sky shader that samples precomputed LUT
    std::unique_ptr<Shader> lightCubeShader;
    std::shared_ptr<Shader> debugFBOShader;
    std::shared_ptr<Shader> cloudShader;
    std::shared_ptr<Shader> cloudCompositeShader;

    // Sky scattering LUT
    std::unique_ptr<SkyLUT> skyLUT;
    
    // CSM
    std::shared_ptr<Shader> csmDepthShader;
	std::shared_ptr<Shader> csmDepthAlphaShader; // alternate shadow program with alpha test
    GLuint csmFBO = 0;
    GLuint csmDepthMaps = 0;
    unsigned int depthMapResolution = 1024;
    float cameraFarPlane = 250.0f;
    std::vector<glm::mat4> csmLightSpaceMatrices;

	void rebuildCSMResources(); // glDeleteTextures + initCSMResources()
	void recomputeCascadeSplits(); // derive shadowCascadeLevels from cascadeCount + farPlane
	PcfQuality pcfQuality = PcfQuality::Low;
	bool shadowAlphaTest = false; // whether to alpha-test shadow casters when rendering depth maps (only relevant for foliage)

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
    float skyTimeOffset = 0.5f;
    bool skyTimePaused = false;

    // Sky mode: 0 = Skyrim (pause/step), 1 = Smooth (linear)
    uint8_t skyMode = 0;
    float skyTimeSpeed = 0.05f;  // sun advancement speed multiplier
    // "Skyrim approach": sun holds still, then jumps forward
    float sunPauseTimer    = 0.0f;   // accumulator (seconds)
    float sunPauseDuration = 20.0f;  // how long the sun stays still
    float sunStepDuration  = 2.0f;   // how long the smooth advance takes
    bool  sunStepping      = false;  // true while the sun is advancing
    float sunStepTimer     = 0.0f;   // progress within the step
    // Simple tone-mapping exposure for sky shader (drives the final composite tonemap in HDR mode).
    float skyExposure = 1.2f;
    // Post-tonemap saturation applied in clouds_composite. Compensates for the
    // midtone desaturation introduced by ACES + the fact that block textures
    // are sRGB-encoded but treated as linear. 1.0 = identity.
    float skySaturation = 1.20f;
    // When true, the scene framebuffer is RGBA16F and tone-mapping happens once
    // at the very end (clouds_composite). When false, the legacy LDR path is used
    // where the sky and the cloud composite each tone-map their own outputs.
    bool  hdrEnabled = true;
    // Atmospheric density and thickness scalars (1.0 ~ Earth-like)
    float skyAtmDensity = 19.0f;
    float skyAtmThickness = 1.0f;
    float planetScale = 7900.0f;
    bool skyLUTEnabled = true;  // Use precomputed scattering LUT (much faster)

    // Cloud controls
    bool cloudsEnabled = true;
    float cloudLayerMinY = 260.0f; // cloud layer altitude (shared between cloud march and sky depth composite)
    float cloudLayerMaxY = 310.0f;
    float cloudDensity = 0.135f; // overall cloud density (increased for more visible clouds)
    float cloudSigmaT = 2.0f; // extinction coefficient (lower = less absorption, brighter clouds)
    glm::vec3 cloudAlbedo = glm::vec3(1.0f); // cloud albedo (reflectivity)
    float cloudStepCount = 48.0f; // number of steps (lower for performance, still good quality)

    float cloudSigmaS = 3.6f; // scattering coefficient
    float cloudPhaseG = 0.4f; // phase function (lower = more uniform scattering, less directional)

    int cloudDownscale = 4; // downscaling factor for cloud rendering (higher = faster but blurrier)

    float cloudEdgeFeather = 10.0f;      // smaller feather = sharper edges
    float cloudNoiseScale = 0.006f;     // lower frequency = bigger, chunkier clouds
    float cloudNoiseContrastLo = 0.59f; // tighter contrast range for more defined shapes
    float cloudNoiseContrastHi = 1.0f;
    float cloudWindSpeed = 100.0f;
    glm::vec2 cloudWindDir = glm::vec2(1.0f, 0.2f); // mostly horizontal drift

    // Underwater params
    glm::vec3 underwaterTintColor = glm::vec3(0.4, 0.85, 0.542);
    glm::vec3 underwaterFogColor = glm::vec3(0.0, 0.091, 0.181);
    float underwaterFogDensity = 0.1f;
    

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

    float seaLevel = 64.0f;

    // DEBUG
    bool showShadowMap = false;
};

#endif