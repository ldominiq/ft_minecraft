#include "WaterFramebuffer.hpp"

WaterFramebuffer::WaterFramebuffer(int width, int height)
    : displayWidth(width), displayHeight(height) {
    initializeReflectionFrameBuffer();
    initializeRefractionFrameBuffer();
}

WaterFramebuffer::~WaterFramebuffer() {
    cleanUp();
}

void WaterFramebuffer::cleanUp() {
    if (reflectionFrameBuffer) {
        glDeleteFramebuffers(1, &reflectionFrameBuffer);
        glDeleteTextures(1, &reflectionTexture);
        glDeleteRenderbuffers(1, &reflectionDepthBuffer);
    }
    if (refractionFrameBuffer) {
        glDeleteFramebuffers(1, &refractionFrameBuffer);
        glDeleteTextures(1, &refractionTexture);
        glDeleteTextures(1, &refractionDepthTexture);
    }
}

void WaterFramebuffer::bindFrameBuffer(GLuint framebuffer, int width, int height) {
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    glViewport(0, 0, width, height);
}

void WaterFramebuffer::bindReflectionFrameBuffer() {
    bindFrameBuffer(reflectionFrameBuffer, REFLECTION_WIDTH, REFLECTION_HEIGHT);
}

void WaterFramebuffer::bindRefractionFrameBuffer() {
    bindFrameBuffer(refractionFrameBuffer, REFRACTION_WIDTH, REFLECTION_HEIGHT);
}

// switch back to default framebuffer
void WaterFramebuffer::unbindCurrentFrameBuffer() const {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, displayWidth, displayHeight);
}

void WaterFramebuffer::initializeReflectionFrameBuffer() {
    reflectionFrameBuffer = createFrameBuffer();
    reflectionTexture = createTextureAttachment(REFLECTION_WIDTH, REFLECTION_HEIGHT);
    reflectionDepthBuffer = createDepthBufferAttachment(REFLECTION_WIDTH, REFLECTION_HEIGHT);
    unbindCurrentFrameBuffer();
}

void WaterFramebuffer::initializeRefractionFrameBuffer() {
    refractionFrameBuffer = createFrameBuffer();
    refractionTexture = createTextureAttachment(REFRACTION_WIDTH, REFRACTION_HEIGHT);
    refractionDepthTexture = createDepthTextureAttachment(REFRACTION_WIDTH, REFRACTION_HEIGHT);
    unbindCurrentFrameBuffer();
}

GLuint WaterFramebuffer::createFrameBuffer() {
    GLuint frameBuffer;
    glGenFramebuffers(1, &frameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, frameBuffer);
    glDrawBuffer(GL_COLOR_ATTACHMENT0);
    return frameBuffer;
}

GLuint WaterFramebuffer::createTextureAttachment(int width, int height) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, texture, 0);
    return texture;
}

GLuint WaterFramebuffer::createDepthTextureAttachment(int width, int height) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, texture, 0);
    return texture;
}

GLuint WaterFramebuffer::createDepthBufferAttachment(int width, int height) {
    GLuint depthBuffer;
    glGenRenderbuffers(1, &depthBuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, depthBuffer);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT, width, height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, depthBuffer);
    return depthBuffer;
}