#ifndef SSAO_HPP
#define SSAO_HPP

#include "Shader.hpp"
#include "GBuffer.hpp"

#include <glad/glad.h>
#include <random>
#include <vector>
#include <glm/glm.hpp>
#include <memory>
#include <algorithm>

class SSAO {
    public:
        SSAO(int width, int height);
        ~SSAO();

        void setEnabled(bool e) { enabled = e; }
        bool isEnabled() const { return enabled; }
        void resize(int w, int h);
        void renderSSAO(const GBuffer& gBuffer, const glm::mat4& projection);
        void blurSSAO();

        /// The final SSAO texture to sample in the lighting pass.
        /// Returns the blurred texture when available and blur enabled, raw otherwise.
        GLuint getSSAOTexture() const { return (blurEnabled && ssaoBlurTexture) ? ssaoBlurTexture : ssaoBuffer; }

        /// The raw (unblurred) SSAO texture
        GLuint getRawSSAOTexture() const { return ssaoBuffer; }

        void setBlurEnabled(bool e) { blurEnabled = e; }
        bool isBlurEnabled() const { return blurEnabled; }

        void setHalfResolution(bool e) { halfResolution = e; resolutionChanged = true; }
        bool isHalfResolution() const { return halfResolution; }

        int getKernelSize() const { return kernelSize; }
        float getRadius() const { return radius; }
        float getBias() const { return bias; }
        float getPower() const { return power; }
        void setKernelSize(int size) { kernelSize = std::min(size, MAX_KERNEL_SIZE); kernelDirty = true; }
        void setRadius(float r) { radius = r; }
        void setBias(float b) { bias = b; }
        void setPower(float p) { power = p; }

    private:
        void generateKernel();
        void generateNoiseTexture();
        void generateFramebuffers();
        void destroyFramebuffers();
        float lerp(float a, float b, float f);
        
        /// Returns the actual SSAO render width (half if halfResolution enabled)
        int getSSAOWidth() const { return halfResolution ? SCR_WIDTH / 2 : SCR_WIDTH; }
        int getSSAOHeight() const { return halfResolution ? SCR_HEIGHT / 2 : SCR_HEIGHT; }

        int SCR_WIDTH;
        int SCR_HEIGHT;
        bool enabled = true;
        bool halfResolution = true;   // Half-res SSAO for ~4x perf gain
        bool resolutionChanged = false;

        // Dirty flags — avoid re-uploading uniforms every frame
        bool kernelDirty = true;
        glm::mat4 cachedProjection{0.0f}; // Zero-init so first comparison always triggers upload

        // Hemisphere sample kernel
        static constexpr int MAX_KERNEL_SIZE = 64;
        int kernelSize = 4;
        std::vector<glm::vec3> ssaoKernel;

        // 4x4 noise texture for random rotation
        GLuint noiseTexture = 0;

        // SSAO FBO (raw occlusion, single-channel grayscale)
        GLuint ssaoFBO = 0;
        GLuint ssaoBuffer = 0;

        // Blur FBO
        GLuint ssaoBlurFBO = 0;
        GLuint ssaoBlurTexture = 0;

        // Shaders
        std::unique_ptr<Shader> ssaoShader;
        std::unique_ptr<Shader> ssaoBlurShader;

        // Fullscreen quad VAO (empty, uses gl_VertexID like sky.vert)
        GLuint quadVAO = 0;

        // Tweakable
        float radius = 3.63f;
        float bias   = 0.088f;
        float power  = 0.25f;
        bool blurEnabled = true;

        
        


};

#endif // SSAO_HPP