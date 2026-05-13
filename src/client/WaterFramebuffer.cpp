#include "WaterFramebuffer.hpp"

WaterFramebuffer::WaterFramebuffer(int width, int height, bool hdr)
    : displayWidth(width), displayHeight(height), hdrEnabled(hdr) {
    initializeReflectionFrameBuffer();
    initializeRefractionFrameBuffer();
}

WaterFramebuffer::~WaterFramebuffer() {
    cleanUp();
}

void WaterFramebuffer::cleanUp() {
    destroyReflection();
    destroyRefraction();
}

void WaterFramebuffer::destroyReflection() {
    if (reflectionFrameBuffer) {
        glDeleteFramebuffers(1, &reflectionFrameBuffer);
        glDeleteTextures(1, &reflectionTexture);
        glDeleteRenderbuffers(1, &reflectionDepthBuffer);
        reflectionFrameBuffer = 0;
        reflectionTexture = 0;
        reflectionDepthBuffer = 0;
    }
}

void WaterFramebuffer::destroyRefraction() {
    if (refractionFrameBuffer) {
        glDeleteFramebuffers(1, &refractionFrameBuffer);
        glDeleteTextures(1, &refractionTexture);
        glDeleteTextures(1, &refractionDepthTexture);
        refractionFrameBuffer = 0;
        refractionTexture = 0;
        refractionDepthTexture = 0;
    }
}

void WaterFramebuffer::setHDR(bool enabled) {
    if (enabled == hdrEnabled) return;
    hdrEnabled = enabled;

    // Rebuild both color attachments with the new internal format. Depth attachments
    // don't depend on HDR but we tear down the whole FBO for simplicity — this only
    // runs when the user toggles the HDR setting, not per frame.
    destroyReflection();
    destroyRefraction();
    initializeReflectionFrameBuffer();
    initializeRefractionFrameBuffer();
    unbindCurrentFrameBuffer();
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
    bindFrameBuffer(refractionFrameBuffer, refractionWidth, refractionHeight);
}

void WaterFramebuffer::resizeRefraction(int width, int height) {
    if (width < 1)  width = 1;
    if (height < 1) height = 1;
    if (width == refractionWidth && height == refractionHeight) return;

    // Tear down the old refraction attachments and FBO, then rebuild at the new size.
    destroyRefraction();

    refractionWidth = width;
    refractionHeight = height;
    initializeRefractionFrameBuffer();
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
    refractionTexture = createTextureAttachment(refractionWidth, refractionHeight);
    refractionDepthTexture = createDepthTextureAttachment(refractionWidth, refractionHeight);
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
    // In HDR mode the scene shader writes linear radiance that can exceed 1.0
    // (dirLight ambient/diffuse boosts in Lighting::uploadLightingUniforms).
    // R11F_G11F_B10F gives us float headroom with the same bandwidth as RGB8
    // and no alpha channel cost — the reflection/refraction passes don't use
    // alpha. water.frag samples this texture and feeds the result into the
    // HDR scene buffer, which the final composite tonemaps once.
    const GLenum internalFmt = hdrEnabled ? GL_R11F_G11F_B10F : GL_RGB8;
    const GLenum pixelType   = hdrEnabled ? GL_HALF_FLOAT      : GL_UNSIGNED_BYTE;
    glTexImage2D(GL_TEXTURE_2D, 0, internalFmt, width, height, 0, GL_RGB, pixelType, nullptr);
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