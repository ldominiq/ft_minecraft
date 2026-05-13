#include "SceneFramebuffer.hpp"
#include <algorithm>
#include <cstdio>

SceneFramebuffer::SceneFramebuffer(int width, int height, int samples_)
: w(std::max(1, width)), h(std::max(1, height)), samples(std::max(1, samples_))
{
    create();
}

SceneFramebuffer::~SceneFramebuffer() { destroy(); }

void SceneFramebuffer::destroy()
{
    if (colorTexMS)      { glDeleteTextures(1, &colorTexMS);      colorTexMS = 0; }
    if (depthTexMS)      { glDeleteTextures(1, &depthTexMS);      depthTexMS = 0; }
    if (fboMS)           { glDeleteFramebuffers(1, &fboMS);       fboMS = 0; }
    if (colorTexResolve) { glDeleteTextures(1, &colorTexResolve); colorTexResolve = 0; }
    if (depthTexResolve) { glDeleteTextures(1, &depthTexResolve); depthTexResolve = 0; }
    if (fboResolve)      { glDeleteFramebuffers(1, &fboResolve);  fboResolve = 0; }
}

void SceneFramebuffer::create()
{
    // --- MSAA FBO: color + depth as multisample textures so we can blit both ---
    glGenFramebuffers(1, &fboMS);
    glBindFramebuffer(GL_FRAMEBUFFER, fboMS);

    glGenTextures(1, &colorTexMS);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, colorTexMS);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_RGBA8, w, h, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, colorTexMS, 0);

    glGenTextures(1, &depthTexMS);
    glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, depthTexMS);
    glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, samples, GL_DEPTH_COMPONENT24, w, h, GL_TRUE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D_MULTISAMPLE, depthTexMS, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[SceneFramebuffer] MSAA FBO incomplete (%dx%d, %dx samples)\n", w, h, samples);
    }

    // --- Resolved FBO: non-MSAA textures, sampleable in shaders ---
    glGenFramebuffers(1, &fboResolve);
    glBindFramebuffer(GL_FRAMEBUFFER, fboResolve);

    glGenTextures(1, &colorTexResolve);
    glBindTexture(GL_TEXTURE_2D, colorTexResolve);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTexResolve, 0);

    glGenTextures(1, &depthTexResolve);
    glBindTexture(GL_TEXTURE_2D, depthTexResolve);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, w, h, 0, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    // Sample as raw depth, not via PCF comparison.
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthTexResolve, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        std::fprintf(stderr, "[SceneFramebuffer] resolve FBO incomplete (%dx%d)\n", w, h);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneFramebuffer::resize(int width, int height)
{
    width  = std::max(1, width);
    height = std::max(1, height);
    if (width == w && height == h) return;
    w = width;
    h = height;
    destroy();
    create();
}

void SceneFramebuffer::bindMS() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, fboMS);
}

void SceneFramebuffer::resolve() const
{
    glBindFramebuffer(GL_READ_FRAMEBUFFER, fboMS);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fboResolve);
    glBlitFramebuffer(0, 0, w, h, 0, 0, w, h,
                      GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SceneFramebuffer::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
