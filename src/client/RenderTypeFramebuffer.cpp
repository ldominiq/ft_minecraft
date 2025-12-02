#include "RenderTypeFramebuffer.hpp"

RenderTypeFramebuffer::RenderTypeFramebuffer(int width, int height)
    : displayWidth(width), displayHeight(height) {
    initializeNormalsFrameBuffer();
    initializeDepthFrameBuffer();
}

RenderTypeFramebuffer::~RenderTypeFramebuffer() {
    cleanUp();
}

void RenderTypeFramebuffer::cleanUp() {
    if (normalsFrameBuffer) {
        glDeleteFramebuffers(1, &normalsFrameBuffer);
        glDeleteTextures(1, &normalsTexture);
        glDeleteRenderbuffers(1, &normalsDepthBuffer);
    }
    if (depthFrameBuffer) {
        glDeleteFramebuffers(1, &depthFrameBuffer);
        glDeleteTextures(1, &depthTexture);
        glDeleteRenderbuffers(1, &depthDepthBuffer);
    }
}

void RenderTypeFramebuffer::bindFrameBuffer(GLuint framebuffer, int width, int height) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, width, height);
}

void RenderTypeFramebuffer::bindNormalsFrameBuffer() {
    bindFrameBuffer(normalsFrameBuffer, DEBUG_WIDTH, DEBUG_HEIGHT);
}

void RenderTypeFramebuffer::bindDepthFrameBuffer() {
    bindFrameBuffer(depthFrameBuffer, DEBUG_WIDTH, DEBUG_HEIGHT);
}

void RenderTypeFramebuffer::unbindCurrentFrameBuffer() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, displayWidth, displayHeight);
}

void RenderTypeFramebuffer::initializeNormalsFrameBuffer() {
    normalsFrameBuffer = createFrameBuffer();
    normalsTexture = createTextureAttachment(DEBUG_WIDTH, DEBUG_HEIGHT);
    normalsDepthBuffer = createDepthBufferAttachment(DEBUG_WIDTH, DEBUG_HEIGHT);
    unbindCurrentFrameBuffer();
}

void RenderTypeFramebuffer::initializeDepthFrameBuffer() {
    depthFrameBuffer = createFrameBuffer();
    depthTexture = createTextureAttachment(DEBUG_WIDTH, DEBUG_HEIGHT);
    depthDepthBuffer = createDepthBufferAttachment(DEBUG_WIDTH, DEBUG_HEIGHT);
    unbindCurrentFrameBuffer();
}

GLuint RenderTypeFramebuffer::createFrameBuffer() {
    GLuint frameBuffer;
    glGenFramebuffers(1, &frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    return frameBuffer;
}

GLuint RenderTypeFramebuffer::createTextureAttachment(int width, int height) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0);
    return texture;
}

GLuint RenderTypeFramebuffer::createDepthBufferAttachment(int width, int height) {
    GLuint depthBuffer;
    glGenRenderbuffers(1, &depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
    return depthBuffer;
}
