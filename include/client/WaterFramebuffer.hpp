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

    void cleanUp();

private:
    static constexpr int REFLECTION_WIDTH = 320;
    static constexpr int REFLECTION_HEIGHT = 180;
    static constexpr int REFRACTION_WIDTH = 1280;
    static constexpr int REFRACTION_HEIGHT = 720;

    GLuint reflectionFrameBuffer;
    GLuint reflectionTexture;
    GLuint reflectionDepthBuffer;

    GLuint refractionFrameBuffer;
    GLuint refractionTexture;
    GLuint refractionDepthTexture;

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
