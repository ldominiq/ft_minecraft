#ifndef SCENE_POST_FB_HPP
#define SCENE_POST_FB_HPP

#include <glad/glad.h>

// Lightweight post-process scratch FBO. Owns a single sampleable depth
// texture sized to the screen. Used by post-process passes that need scene
// depth (god rays, lens flare) when the GBuffer's depth isn't available
// (e.g. SSAO disabled). The depth is populated each frame via a blit from
// the default framebuffer's depth attachment immediately after the main
// scene pass.
class ScenePostFB {
public:
    ScenePostFB(int width, int height);
    ~ScenePostFB();

    void resize(int w, int h);

    // Blit depth from the currently-bound *read* framebuffer (typically the
    // default framebuffer right after renderScene) into our sampleable
    // depth texture. Leaves the read/draw bindings as they were.
    void captureDepthFromDefault();

    GLuint getDepthTexture() const { return depthTex; }
    int    getWidth()  const { return SCR_WIDTH; }
    int    getHeight() const { return SCR_HEIGHT; }

private:
    void create();
    void destroy();

    GLuint fbo      = 0;
    GLuint depthTex = 0;
    int SCR_WIDTH   = 0;
    int SCR_HEIGHT  = 0;
};

#endif // SCENE_POST_FB_HPP
