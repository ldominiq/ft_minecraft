#include "AutoExposure.hpp"
#include "Shader.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <GLFW/glfw3.h>

AutoExposure::AutoExposure() {
    reduceShader = std::make_unique<Shader>("shaders/luminance_reduce.vert",
                                            "shaders/luminance_reduce.frag");

    // Fullscreen-triangle VAO (no attributes).
    glGenVertexArrays(1, &vao);

    // Reduce target: single-channel R16F. Linear filter so the shader's nine taps
    // can also cheaply sample between texels if we ever shrink the offset radius.
    glGenTextures(1, &reduceTex);
    glBindTexture(GL_TEXTURE_2D, reduceTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, REDUCE_SIZE, REDUCE_SIZE, 0, GL_RED, GL_HALF_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &reduceFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, reduceFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, reduceTex, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Two PBOs ping-pong: write one this frame, read the other (filled last frame).
    // Sized for R16F = 2 bytes/pixel.
    glGenBuffers(PBO_COUNT, pbo);
    for (int i = 0; i < PBO_COUNT; ++i) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[i]);
        glBufferData(GL_PIXEL_PACK_BUFFER,
                     REDUCE_SIZE * REDUCE_SIZE * sizeof(uint16_t),
                     nullptr, GL_STREAM_READ);
    }
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
}

AutoExposure::~AutoExposure() {
    if (glfwGetCurrentContext()) {
        if (vao)        glDeleteVertexArrays(1, &vao);
        if (reduceTex)  glDeleteTextures(1, &reduceTex);
        if (reduceFBO)  glDeleteFramebuffers(1, &reduceFBO);
        for (int i = 0; i < PBO_COUNT; ++i) {
            if (pbo[i]) glDeleteBuffers(1, &pbo[i]);
        }
    }
}

void AutoExposure::submit(GLuint hdrSceneTex, int srcW, int srcH) {
    if (!hdrSceneTex || srcW <= 0 || srcH <= 0) return;

    // Save current viewport so we restore after our tiny pass.
    GLint prevViewport[4];
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    // 1) Reduce: HDR scene -> 64x64 log-luminance texture.
    glBindFramebuffer(GL_FRAMEBUFFER, reduceFBO);
    glViewport(0, 0, REDUCE_SIZE, REDUCE_SIZE);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    reduceShader->use();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, hdrSceneTex);
    reduceShader->setInt("hdrScene", 0);
    reduceShader->setVec2("invSrcResolution",
                          glm::vec2(1.0f / static_cast<float>(srcW),
                                    1.0f / static_cast<float>(srcH)));

    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    // 2) Async readback into the current PBO.
    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[pboIdx]);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glReadPixels(0, 0, REDUCE_SIZE, REDUCE_SIZE, GL_RED, GL_HALF_FLOAT, nullptr);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    pboFilled[pboIdx] = true;

    // Swap: next frame writes the other PBO, this frame reads it later in update().
    pboIdx = (pboIdx + 1) % PBO_COUNT;

    // Restore state.
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
}

// Convert a single GL_HALF_FLOAT (uint16_t) to float. Standard IEEE 754 binary16.
static float halfToFloat(uint16_t h) {
    const uint32_t sign  = (h >> 15) & 0x1u;
    const uint32_t exp   = (h >> 10) & 0x1Fu;
    const uint32_t mant  =  h        & 0x3FFu;
    uint32_t f;
    if (exp == 0) {
        if (mant == 0) {
            f = sign << 31;
        } else {
            // Subnormal: normalize.
            uint32_t e = 1;
            uint32_t m = mant;
            while ((m & 0x400u) == 0) { m <<= 1; ++e; }
            m &= 0x3FFu;
            f = (sign << 31) | ((127 - 15 - e + 1) << 23) | (m << 13);
        }
    } else if (exp == 0x1F) {
        f = (sign << 31) | 0x7F800000u | (mant << 13); // Inf or NaN
    } else {
        f = (sign << 31) | ((exp + (127 - 15)) << 23) | (mant << 13);
    }
    float out;
    std::memcpy(&out, &f, sizeof(out));
    return out;
}

float AutoExposure::update(float dt, float currentExposure) {
    // Read the PBO that pboIdx now points to (filled in the *previous* frame).
    if (!pboFilled[pboIdx]) return currentExposure;

    glBindBuffer(GL_PIXEL_PACK_BUFFER, pbo[pboIdx]);
    const void* mapped = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0,
                                          REDUCE_SIZE * REDUCE_SIZE * sizeof(uint16_t),
                                          GL_MAP_READ_BIT);
    if (!mapped) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        return currentExposure;
    }

    const uint16_t* halfs = static_cast<const uint16_t*>(mapped);
    double sum = 0.0;
    constexpr int N = REDUCE_SIZE * REDUCE_SIZE;
    for (int i = 0; i < N; ++i) sum += halfToFloat(halfs[i]);
    const float avgLogLum = static_cast<float>(sum / N);

    glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    const float avgLum = std::exp(avgLogLum);
    lastAvgLum = avgLum;

    const float desiredExposure = std::clamp(targetLuminance / std::max(avgLum, 1e-4f),
                                             minExposure, maxExposure);
    const float desiredLogExp   = std::log(desiredExposure);
    const float currentLogExp   = std::log(std::max(currentExposure, 1e-4f));

    if (!smoothInit) {
        smoothedLogLum = desiredLogExp;
        smoothInit = true;
    } else {
        // Asymmetric rate: dark→bright shuts the iris faster than the reverse.
        const float rate = (desiredLogExp < smoothedLogLum) ? adaptSpeedUp : adaptSpeedDown;
        const float a    = 1.0f - std::exp(-rate * std::max(dt, 0.0f));
        // Lerp toward desired log-exposure. Start from current (not previous smoothed)
        // so external manual overrides take effect immediately when auto-exp toggles back on.
        const float baseLog = currentLogExp;
        smoothedLogLum = baseLog + (desiredLogExp - baseLog) * a;
    }

    return std::exp(smoothedLogLum);
}
