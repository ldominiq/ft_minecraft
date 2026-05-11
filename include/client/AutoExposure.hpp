#ifndef AUTO_EXPOSURE_HPP
#define AUTO_EXPOSURE_HPP

#include <glad/glad.h>
#include <memory>

class Shader;

// PBO-based auto-exposure: reduce the HDR scene into a small log-luminance
// texture each frame, asynchronously read it back with a 1-frame latency, and
// EMA-smooth the resulting average toward a target middle-gray in log space.
//
// Owned by App. Lifecycle:
//   - construct once after the GL context is live
//   - call submit() each frame AFTER sceneFBO->resolve() and BEFORE the
//     cloud composite (the resolved color is what we meter on)
//   - call update(dt, currentExposure) to get the new smoothed exposure
//     (will be 1 frame behind submit() — which is fine and free)
class AutoExposure {
public:
    AutoExposure();
    ~AutoExposure();

    AutoExposure(const AutoExposure&) = delete;
    AutoExposure& operator=(const AutoExposure&) = delete;

    // Render the log-luminance reduce pass. hdrSceneTex must be the resolved
    // RGBA16F scene color. (srcW, srcH) are the source resolution used to size
    // the texel-offset jitter in the reduce shader.
    void submit(GLuint hdrSceneTex, int srcW, int srcH);

    // Advance smoothing by dt and return the new exposure. The first call after
    // construction returns currentExposure unchanged (PBO is still empty).
    float update(float dt, float currentExposure);

    // Tunable parameters (exposed in the debug window).
    float targetLuminance = 0.18f;   // middle-gray target (log-space midpoint)
    float minExposure     = 0.4f;
    // Capped at ~1.5× so the meter can't make a dark night scene look like daylight
    float maxExposure     = 1.5f;
    float adaptSpeedUp    = 1.5f;    // s^-1, eyes-shutting (highlights)
    float adaptSpeedDown  = 0.5f;    // s^-1, eyes-opening (dark adaptation slower)

    // For debug HUD: the most recent measured average scene luminance (linear).
    float getLastAvgLuminance() const { return lastAvgLum; }

private:
    static constexpr int REDUCE_SIZE = 64; // 64x64 reduce target
    static constexpr int PBO_COUNT   = 2;

    GLuint vao = 0;
    GLuint reduceFBO = 0;
    GLuint reduceTex = 0;                  // R16F, REDUCE_SIZE x REDUCE_SIZE
    GLuint pbo[PBO_COUNT] = { 0, 0 };
    int    pboIdx = 0;
    bool   pboFilled[PBO_COUNT] = { false, false };

    std::unique_ptr<Shader> reduceShader;

    float smoothedLogLum = 0.0f;
    bool  smoothInit = false;
    float lastAvgLum = 0.18f;
};

#endif // AUTO_EXPOSURE_HPP
