#ifndef SSAO_HPP
#define SSAO_HPP

#include "Shader.hpp"
#include "GBuffer.hpp"

#include <glad/glad.h>
#include <random>
#include <vector>
#include <glm/glm.hpp>
#include <memory>

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
        GLuint getSSAOTexture() const { return (blurEnabled && ssaoBlurTexture) ? ssaoBlurTexture : ssaoColorBuffer; }

        /// The raw (unblurred) SSAO texture
        GLuint getRawSSAOTexture() const { return ssaoColorBuffer; }

        void setBlurEnabled(bool e) { blurEnabled = e; }
        bool isBlurEnabled() const { return blurEnabled; }

        int getKernelSize() const { return kernelSize; }
        float getRadius() const { return radius; }
        float getBias() const { return bias; }
        float getPower() const { return power; }
        void setKernelSize(int size) { kernelSize = std::min(size, MAX_KERNEL_SIZE); generateKernel(); }
        void setRadius(float r) { radius = r; }
        void setBias(float b) { bias = b; }
        void setPower(float p) { power = p; }

    private:
        void generateKernel();
        void generateNoiseTexture();
        void generateFramebuffers();
        void destroyFramebuffers();
        float lerp(float a, float b, float f);
        
        int SCR_WIDTH;
        int SCR_HEIGHT;
        bool enabled = true;

        // Hemisphere sample kernel
        static constexpr int MAX_KERNEL_SIZE = 64;
        int kernelSize = 64;
        std::vector<glm::vec3> ssaoKernel;

        // 4x4 noise texture for random rotation
        GLuint noiseTexture = 0;

        // SSAO FBO (raw occlusion, single-channel)
        GLuint ssaoFBO = 0;
        GLuint ssaoColorBuffer = 0;

        // Blur FBO
        GLuint ssaoBlurFBO = 0;
        GLuint ssaoBlurTexture = 0;

        // Shaders
        std::unique_ptr<Shader> ssaoShader;
        std::unique_ptr<Shader> ssaoBlurShader;

        // Fullscreen quad VAO (empty, uses gl_VertexID like sky.vert)
        GLuint quadVAO = 0;

        // Tweakable
        float radius = 0.5f;
        float bias   = 0.025f;
        float power  = 1.0f;
        bool blurEnabled = true;

        
        


};

#endif // SSAO_HPP