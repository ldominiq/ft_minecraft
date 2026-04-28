#ifndef WATERFRAMEBUFFER_HPP
#define WATERFRAMEBUFFER_HPP

#include <glad/glad.h>

class WaterFramebuffer {
public:
    WaterFramebuffer(int width, int height);
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

    void initializeReflectionFrameBuffer();
    void initializeRefractionFrameBuffer();
    GLuint createFrameBuffer();
    GLuint createTextureAttachment(int width, int height);
    GLuint createDepthTextureAttachment(int width, int height);
    GLuint createDepthBufferAttachment(int width, int height);
};

#endif
