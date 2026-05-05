#include "GodRays.hpp"
#include "Lighting.hpp"
#include "TextureUnits.hpp"

#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <iostream>
#include <random>
#include <vector>

GodRays::GodRays(int width, int height) : SCR_WIDTH(width), SCR_HEIGHT(height) {
    rayMarchShader = std::make_unique<Shader>(
        "shaders/godRays.vert", "shaders/godRays.frag");
    compositeShader = std::make_unique<Shader>(
        "shaders/godRays.vert", "shaders/godRaysComposite.frag");

    glGenVertexArrays(1, &quadVAO);

    generateBlueNoise();
    create();
}

GodRays::~GodRays() {
    destroy();
    if (blueNoiseTex) glDeleteTextures(1, &blueNoiseTex);
    if (quadVAO)      glDeleteVertexArrays(1, &quadVAO);
}

void GodRays::create() {
    const int w = std::max(1, SCR_WIDTH  / 2);
    const int h = std::max(1, SCR_HEIGHT / 2);

    glGenFramebuffers(1, &halfResFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, halfResFBO);

    glGenTextures(1, &halfResColor);
    glBindTexture(GL_TEXTURE_2D, halfResColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, halfResColor, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::GOD_RAYS::FRAMEBUFFER_NOT_COMPLETE" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void GodRays::destroy() {
    if (halfResFBO)   { glDeleteFramebuffers(1, &halfResFBO); halfResFBO = 0; }
    if (halfResColor) { glDeleteTextures(1, &halfResColor);   halfResColor = 0; }
}

void GodRays::resize(int w, int h) {
    if (w == SCR_WIDTH && h == SCR_HEIGHT) return;
    SCR_WIDTH = w;
    SCR_HEIGHT = h;
    destroy();
    create();
}

void GodRays::generateBlueNoise() {
    // Cheap interleaved-gradient noise baked into a 64x64 R8 tile. Not true
    // blue-noise but good enough to mask 32-step banding when combined with
    // the bilinear upsample.
    const int N = 64;
    std::vector<unsigned char> data(N * N);
    for (int y = 0; y < N; ++y) {
        for (int x = 0; x < N; ++x) {
            float v = 52.9829189f * std::fmod(0.06711056f * float(x) + 0.00583715f * float(y), 1.0f);
            v = v - std::floor(v);
            data[y * N + x] = static_cast<unsigned char>(v * 255.0f);
        }
    }
    glGenTextures(1, &blueNoiseTex);
    glBindTexture(GL_TEXTURE_2D, blueNoiseTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, N, N, 0, GL_RED, GL_UNSIGNED_BYTE, data.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

void GodRays::render(const Lighting& lighting,
                     const glm::mat4& projection,
                     const glm::mat4& view,
                     const glm::vec3& sunDirToward,
                     const glm::vec3& sunColor,
                     GLuint sceneDepthTex,
                     int screenWidth,
                     int screenHeight)
{
    if (!enabled) return;
    resize(screenWidth, screenHeight);

    // Rotation-only view to keep everything in camera-relative space (matches
    // the convention used by lighting.frag's CSMShadowCalculation).
    glm::mat4 viewRot = view;
    viewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
    const glm::mat4 invProjViewRot = glm::inverse(projection * viewRot);

    // ── Pass 1: half-res ray-march ──────────────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, halfResFBO);
    glViewport(0, 0, SCR_WIDTH / 2, SCR_HEIGHT / 2);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    rayMarchShader->use();

    // Re-uses uploadCSMUniforms to bind the shadow map array + cascade data.
    lighting.uploadCSMUniforms(*rayMarchShader, view);

    // Scene depth on a free unit (CSM uses unit 7, SKY_LUT uses 9).
    const int kDepthUnit = 6;
    glActiveTexture(GL_TEXTURE0 + kDepthUnit);
    glBindTexture(GL_TEXTURE_2D, sceneDepthTex);
    rayMarchShader->setInt("sceneDepth", kDepthUnit);

    // Blue-noise jitter on a free unit.
    const int kNoiseUnit = 10;
    glActiveTexture(GL_TEXTURE0 + kNoiseUnit);
    glBindTexture(GL_TEXTURE_2D, blueNoiseTex);
    rayMarchShader->setInt("blueNoise", kNoiseUnit);

    rayMarchShader->setMat4("invProjViewRot", invProjViewRot);
    rayMarchShader->setVec3("sunDir",         glm::normalize(sunDirToward));
    rayMarchShader->setVec3("sunColor",       sunColor);
    rayMarchShader->setInt ("numSteps",       numSteps);
    rayMarchShader->setFloat("density",       density);
    rayMarchShader->setFloat("anisotropy",    anisotropy);
    rayMarchShader->setFloat("maxDistance",   maxDistance);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // ── Pass 2: full-res additive composite ─────────────────────────────
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    // Depth state: god rays should not occlude or be occluded; just composite.
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    compositeShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, halfResColor);
    compositeShader->setInt("scatterTex", 0);
    compositeShader->setFloat("intensity", intensity);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);

    // Restore conservative state for whatever runs next. blendFunc must be
    // reset because subsequent passes may enable GL_BLEND without rebinding
    // the function and would otherwise inherit our additive setting.
    glDisable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glBindVertexArray(0);
}
