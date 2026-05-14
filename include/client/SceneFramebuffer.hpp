#ifndef SCENE_FRAMEBUFFER_HPP
#define SCENE_FRAMEBUFFER_HPP

#include <glad/glad.h>

// Offscreen framebuffer for the main scene render.
// Stores an MSAA color+depth target (matched to the GLFW window's sample count)
// and a non-MSAA "resolved" pair whose color/depth textures are sampleable by
// the post-terrain cloud composite shader.
//
// Usage per frame:
//   sceneFBO.bindMS();              // bind MSAA FBO, render scene normally
//   ... scene draws ...
//   sceneFBO.resolve();             // blit MSAA -> resolved (color + depth)
//   SceneFramebuffer::unbind();     // back to FBO=0
//   // sample sceneFBO.getResolvedColorTexture()/getResolvedDepthTexture() in a fullscreen pass
class SceneFramebuffer {
public:
    // hdr=true uses GL_RGBA16F for color (linear HDR radiance). hdr=false keeps
    // the legacy GL_RGBA8 path so the LDR fallback is bit-identical.
    SceneFramebuffer(int width, int height, int samples = 8, bool hdr = false);
    ~SceneFramebuffer();

    SceneFramebuffer(const SceneFramebuffer&) = delete;
    SceneFramebuffer& operator=(const SceneFramebuffer&) = delete;

    void resize(int width, int height);
    // Switch between HDR (RGBA16F) and LDR (RGBA8). Recreates FBOs if the flag changes.
    void setHDR(bool enabled);
    bool isHDR() const { return hdrEnabled; }

    // Bind the MSAA framebuffer for scene rendering.
    void bindMS() const;
    // Blit MSAA color+depth -> resolved (non-MSAA) textures.
    void resolve() const;
    static void unbind();

    GLuint getResolvedColorTexture() const { return colorTexResolve; }
    GLuint getResolvedDepthTexture() const { return depthTexResolve; }
    int getWidth() const { return w; }
    int getHeight() const { return h; }
    int getSamples() const { return samples; }

private:
    void create();
    void destroy();

    int w = 0;
    int h = 0;
    int samples = 8;
    bool hdrEnabled = false;

    // MSAA target (rendered into by the scene)
    GLuint fboMS = 0;
    GLuint colorTexMS = 0;     // GL_TEXTURE_2D_MULTISAMPLE, GL_RGBA8 or GL_RGBA16F
    GLuint depthTexMS = 0;     // GL_TEXTURE_2D_MULTISAMPLE, GL_DEPTH_COMPONENT24

    // Non-MSAA resolved target (sampled by composite shader)
    GLuint fboResolve = 0;
    GLuint colorTexResolve = 0; // GL_TEXTURE_2D, GL_RGBA8 or GL_RGBA16F
    GLuint depthTexResolve = 0; // GL_TEXTURE_2D, GL_DEPTH_COMPONENT24
};

#endif // SCENE_FRAMEBUFFER_HPP
