#ifndef GOD_RAYS_HPP
#define GOD_RAYS_HPP

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>

#include "Shader.hpp"

class Lighting;

// Half-resolution screen-space volumetric god rays.
//
// Pipeline:
//   1) ray-march pass at half-res, sampling the CSM shadow map each step,
//      writes RGB16F in-scatter into our half-res FBO.
//   2) composite pass at full-res, samples the half-res result with a
//      depth-aware bilinear upsample, additively blends onto the default
//      framebuffer.
//
// The depth texture passed to render() must be sampleable scene depth
// matching the projection used for the main pass (typically the GBuffer
// depth when SSAO is on, otherwise the ScenePostFB blit).
class GodRays {
public:
    GodRays(int width, int height);
    ~GodRays();

    void resize(int w, int h);

    // Render the half-res scattering buffer + composite onto the currently-
    // bound (default) framebuffer. Caller must `lighting.uploadCSMUniforms`
    // on a shader of its own first — but we re-upload CSM bindings here on
    // our shader. `sceneDepthTex` is full-resolution sampleable depth.
    void render(const Lighting& lighting,
                const glm::mat4& projection,
                const glm::mat4& view,
                const glm::vec3& sunDirToward,
                const glm::vec3& sunColor,
                GLuint sceneDepthTex,
                int screenWidth,
                int screenHeight);

    // --- toggles / params ---
    void setEnabled(bool e)        { enabled = e; }
    bool isEnabled() const         { return enabled; }
    void setNumSteps(int n)        { numSteps = n; }
    int  getNumSteps() const       { return numSteps; }
    void setDensity(float d)       { density = d; }
    float getDensity() const       { return density; }
    void setAnisotropy(float g)    { anisotropy = g; }
    float getAnisotropy() const    { return anisotropy; }
    void setMaxDistance(float d)   { maxDistance = d; }
    float getMaxDistance() const   { return maxDistance; }
    void setIntensity(float i)     { intensity = i; }
    float getIntensity() const     { return intensity; }

private:
    void create();
    void destroy();
    void generateBlueNoise();

    int SCR_WIDTH  = 0;
    int SCR_HEIGHT = 0;
    bool enabled = true;

    // Tunables. Defaults aim for "subtle shafts between leaves" rather than
    // a global fog. Density above ~0.02 will start to read as a sun halo
    // because the Mie phase peaks ~30x at the sun direction.
    int   numSteps    = 32;
    float density     = 0.012f;
    float anisotropy  = 0.55f;
    float maxDistance = 200.0f;
    float intensity   = 1.0f;

    // Half-res FBO (RGB16F). No depth attachment needed.
    GLuint halfResFBO    = 0;
    GLuint halfResColor  = 0;

    // Procedural blue-noise tile (64x64, R8) for ray-march jitter.
    GLuint blueNoiseTex  = 0;

    // Shaders + empty VAO for fullscreen triangle.
    std::unique_ptr<Shader> rayMarchShader;
    std::unique_ptr<Shader> compositeShader;
    GLuint quadVAO = 0;
};

#endif // GOD_RAYS_HPP
