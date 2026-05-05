#ifndef LENS_FLARE_HPP
#define LENS_FLARE_HPP

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>

#include "Shader.hpp"

// Sprite-based lens flare. One full-screen pass that:
//   1) projects the sun world-direction to screen,
//   2) samples scene depth at + around the projected point to compute a
//      visibility factor (sky == lit),
//   3) draws a chain of procedural ghost/halo splats along
//      (sunScreenPos -> screenCenter) and additively blends.
//
// No texture assets are required — the ghosts are generated procedurally
// inside the fragment shader from Gaussian falloffs.
class LensFlare {
public:
    LensFlare();
    ~LensFlare();

    void render(const glm::mat4& viewProj,
                const glm::vec3& sunDirToward,
                GLuint sceneDepthTex,
                int screenWidth,
                int screenHeight);

    void setEnabled(bool e)    { enabled = e; }
    bool isEnabled() const     { return enabled; }
    void setIntensity(float i) { intensity = i; }
    float getIntensity() const { return intensity; }

private:
    bool enabled = true;
    float intensity = 1.0f;

    std::unique_ptr<Shader> flareShader;
    GLuint quadVAO = 0;
};

#endif // LENS_FLARE_HPP
