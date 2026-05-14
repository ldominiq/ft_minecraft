#ifndef WATERFRAMEBUFFER_HPP
#define WATERFRAMEBUFFER_HPP

#include <glad/glad.h>

class WaterFramebuffer {
public:
    WaterFramebuffer(int width, int height, bool hdr = false);
    ~WaterFramebuffer();

    void bindFrameBuffer(GLuint framebuffer, int width, int height);
    void bindReflectionFrameBuffer();
    void bindRefractionFrameBuffer();
    void unbindCurrentFrameBuffer() const;

    GLuint getReflectionTexture() const { return reflectionTexture; }
    GLuint getRefractionTexture() const { return refractionTexture; }
    GLuint getRefractionDepthTexture() const { return refractionDepthTexture; }

    int getWidth() const { return displayWidth; }
    int getHeight() const { return displayHeight; }

    int getRefractionWidth() const { return refractionWidth; }
    int getRefractionHeight() const { return refractionHeight; }

    bool isHDR() const { return hdrEnabled; }

    /// Switch the reflection + refraction color attachments between LDR (GL_RGB8)
    /// and HDR (GL_R11F_G11F_B10F). Must match the scene shader's hdrMode so the
    /// linear-HDR radiance the scene writes here doesn't clamp before water.frag
    /// samples it. Rebuilds both FBOs on transition.
    void setHDR(bool enabled);

    /// Recreate the refraction FBO + textures at a new resolution. Cheap (one-shot
    /// per setting change), called from WaterRenderer::setRefractionResolutionScale.
    void resizeRefraction(int width, int height);

    void cleanUp();

private:
    static constexpr int REFLECTION_WIDTH = 320;
    static constexpr int REFLECTION_HEIGHT = 180;
    int refractionWidth = 1280;
    int refractionHeight = 720;

    GLuint reflectionFrameBuffer = 0;
    GLuint reflectionTexture = 0;
    GLuint reflectionDepthBuffer = 0;

    GLuint refractionFrameBuffer = 0;
    GLuint refractionTexture = 0;
    GLuint refractionDepthTexture = 0;

    int displayWidth;
    int displayHeight;
    bool hdrEnabled;

    void initializeReflectionFrameBuffer();
    void initializeRefractionFrameBuffer();
    void destroyReflection();
    void destroyRefraction();
    GLuint createFrameBuffer();
    GLuint createTextureAttachment(int width, int height);
    GLuint createDepthTextureAttachment(int width, int height);
    GLuint createDepthBufferAttachment(int width, int height);
};

#endif
