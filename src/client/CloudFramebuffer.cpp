#include "CloudFramebuffer.hpp"
#include <algorithm>

CloudFramebuffer::CloudFramebuffer(int screenWidth, int screenHeight, int downscale_)
: screenW(screenWidth), screenH(screenHeight), downscale(std::max(1, downscale_))
{
    create();
}

CloudFramebuffer::~CloudFramebuffer() { destroy(); }

void CloudFramebuffer::destroy()
{
    if (depthRbo) { glDeleteRenderbuffers(1, &depthRbo); depthRbo = 0; }
    if (colorTex) { glDeleteTextures(1, &colorTex); colorTex = 0; }
    if (fbo)      { glDeleteFramebuffers(1, &fbo); fbo = 0; }
}

void CloudFramebuffer::create()
{
    w = std::max(1, screenW / downscale);
    h = std::max(1, screenH / downscale);

    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &colorTex);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, w, h, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, colorTex, 0);

    glGenRenderbuffers(1, &depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, w, h);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthRbo);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void CloudFramebuffer::resize(int screenWidth, int screenHeight)
{
    screenW = screenWidth;
    screenH = screenHeight;
    destroy();
    create();
}

void CloudFramebuffer::bind() const
{
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
}

void CloudFramebuffer::unbind()
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}