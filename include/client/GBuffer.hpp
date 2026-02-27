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

        GLuint getPositionTexture() const { return gPosition; }
        GLuint getNormalTexture()   const { return gNormal; }
        GLuint getDepthTexture()    const { return depthRbo; }

        
    private:
        void create();
        
        GLuint fbo = 0; // Framebuffer object
        GLuint gPosition = 0;
        GLuint gNormal = 0;     // RGB16F — view-space normal
        GLuint depthRbo = 0;    // depth renderbuffer

        int SCR_WIDTH = 0;
        int SCR_HEIGHT = 0;

};

#endif // GBUFFER_HPP