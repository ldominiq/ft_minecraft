#ifndef RENDERTYPEFRAMEBUFFER_HPP
#define RENDERTYPEFRAMEBUFFER_HPP

#include <glad/glad.h>

class RenderTypeFramebuffer {
public:
    RenderTypeFramebuffer(int width, int height);
    ~RenderTypeFramebuffer();

    void bindNormalsFrameBuffer();
    void bindDepthFrameBuffer();
    void unbindCurrentFrameBuffer() const;

    GLuint getNormalsTexture() const { return normalsTexture; }
    GLuint getDepthTexture() const { return depthTexture; }

    void cleanUp();

private:
    static constexpr int DEBUG_WIDTH = 640;
    static constexpr int DEBUG_HEIGHT = 360;

    GLuint normalsFrameBuffer;
    GLuint normalsTexture;
    GLuint normalsDepthBuffer;

    GLuint depthFrameBuffer;
    GLuint depthTexture;
    GLuint depthDepthBuffer;

    int displayWidth;
    int displayHeight;

    void initializeNormalsFrameBuffer();
    void initializeDepthFrameBuffer();
    void bindFrameBuffer(GLuint framebuffer, int width, int height);
    GLuint createFrameBuffer();
    GLuint createTextureAttachment(int width, int height);
    GLuint createDepthBufferAttachment(int width, int height);
};

#endif
