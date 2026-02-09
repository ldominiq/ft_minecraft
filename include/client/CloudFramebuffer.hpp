#ifndef CLOUD_FRAMEBUFFER_HPP
#define CLOUD_FRAMEBUFFER_HPP

#include <glad/glad.h>

class CloudFramebuffer {
public:
    CloudFramebuffer(int screenWidth, int screenHeight, int downscale = 4);
    ~CloudFramebuffer();

    CloudFramebuffer(const CloudFramebuffer&) = delete;
    CloudFramebuffer& operator=(const CloudFramebuffer&) = delete;

    void resize(int screenWidth, int screenHeight);

    void bind() const;
    static void unbind();

    GLuint getColorTexture() const { return colorTex; }
    int getWidth() const { return w; }
    int getHeight() const { return h; }
    int getDownscale() const { return downscale; }

private:
    void create();
    void destroy();

    int screenW = 0;
    int screenH = 0;
    int w = 0;
    int h = 0;
    int downscale = 4;

    GLuint fbo = 0;
    GLuint colorTex = 0;
    GLuint depthRbo = 0;
};

#endif // CLOUD_FRAMEBUFFER_HPP