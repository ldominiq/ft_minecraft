#ifndef GBUFFER_HPP
#define GBUFFER_HPP

#include <glad/glad.h>

class GBuffer {
    public:
        GBuffer(int width, int height);
        ~GBuffer();

        void bind() const;
        void unbind();
        void resize(int w, int h);
        void destroy();

        GLuint getColorTexture()    const { return gColor; }
        GLuint getPositionTexture() const { return gPosition; }
        GLuint getNormalTexture()   const { return gNormal; }
        GLuint getDepthTexture()    const { return depthTexture; }
        GLuint getFBO()             const { return fbo; }


    private:
        void create();
        
        GLuint fbo = 0; // Framebuffer object
        GLuint gColor = 0;       // RGBA8  — scene color (attachment 0)
        GLuint gPosition = 0;    // RGBA16F — view-space position (attachment 1)
        GLuint gNormal = 0;      // RGBA16F — view-space normal (attachment 2)
        GLuint depthTexture = 0; // depth texture (sampleable, replaces renderbuffer)

        int SCR_WIDTH = 0;
        int SCR_HEIGHT = 0;

};

#endif // GBUFFER_HPP