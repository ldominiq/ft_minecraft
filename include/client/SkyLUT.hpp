#ifndef SKYLUT_HPP
#define SKYLUT_HPP

#include "Shader.hpp"

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <memory>

/**
 * Precomputed atmospheric scattering LUT for the sky.
 *
 * Generates a 2D texture parameterized by (viewZenithCos, sunZenithCos).
 * The LUT stores Rayleigh (RGB) and Mie (A) in-scatter with coefficients
 * pre-multiplied, so the final sky shader only needs a single texture
 * lookup + phase function evaluation per fragment.
 *
 * The LUT is regenerated only when atmosphere parameters change
 * (sun direction moves, density/thickness tweaked, etc.).
 */
class SkyLUT {
public:
    SkyLUT(int lutWidth = 256, int lutHeight = 128);
    ~SkyLUT();

    /// Regenerate the LUT if parameters have changed. Call once per frame.
    /// Returns true if the LUT was actually regenerated.
    bool update(float atmDensity, float atmThickness, int viewportWidth, int viewportHeight);

    /// Force a regeneration next frame (e.g. after parameter changes)
    void invalidate() { dirty = true; }

    /// The precomputed LUT texture to bind in the sky render shader
    GLuint getLUTTexture() const { return lutTexture; }

    int getWidth()  const { return LUT_WIDTH; }
    int getHeight() const { return LUT_HEIGHT; }

private:
    void createResources();
    void destroyResources();

    int LUT_WIDTH;
    int LUT_HEIGHT;

    GLuint lutFBO = 0;
    GLuint lutTexture = 0;
    GLuint quadVAO = 0;

    std::unique_ptr<Shader> lutShader;

    // Cached parameters to detect changes
    float cachedAtmDensity = -1.0f;
    float cachedAtmThickness = -1.0f;
    bool dirty = true;
};

#endif // SKYLUT_HPP
