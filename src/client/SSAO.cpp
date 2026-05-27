#include "SSAO.hpp"

SSAO::SSAO(int width, int height) : SCR_WIDTH(width), SCR_HEIGHT(height) {
    ssaoShader  = std::make_unique<Shader>("shaders/ssao.vert", "shaders/ssao.frag");
    ssaoBlurShader = std::make_unique<Shader>("shaders/ssao.vert", "shaders/ssaoBlur.frag");

    // Empty VAO for fullscreen triangle (uses gl_VertexID)
    glGenVertexArrays(1, &quadVAO);

    generateKernel();
    generateNoiseTexture();
    generateFramebuffers();
}

SSAO::~SSAO() {
    destroyFramebuffers();
    if (noiseTexture) glDeleteTextures(1, &noiseTexture);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
}

void SSAO::generateKernel() {

    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
    std::default_random_engine generator;

    ssaoKernel.clear();
    ssaoKernel.reserve(MAX_KERNEL_SIZE);

    for (unsigned int i = 0; i < MAX_KERNEL_SIZE; ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0 - 1.0, // x: random float in range [-1.0, 1.0]
            randomFloats(generator) * 2.0 - 1.0, // y: random float in range [-1.0, 1.0]
            randomFloats(generator)              // z: random float in range [ 0.0, 1.0]
        );
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);

        float scale = (float)i / (float)MAX_KERNEL_SIZE;
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;

        ssaoKernel.push_back(sample);
    }

    kernelDirty = true;
}

// By introducing some randomness onto the sample kernels we largely reduce the number of samples necessary to get good results.
void SSAO::generateNoiseTexture() {

    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
    std::default_random_engine generator;
    std::vector<glm::vec3> ssaoNoise;
    ssaoNoise.reserve(16);

    for (unsigned int i = 0; i < 16; i++) {
        glm::vec3 noise = glm::normalize(glm::vec3(
            randomFloats(generator) * 2.0 - 1.0, // x: random float in range [-1.0, 1.0]
            randomFloats(generator) * 2.0 - 1.0, // y: random float in range [-1.0, 1.0]
            // As the sample kernel is oriented along the positive z direction in tangent space, we leave the z component at 0.0 so we rotate around the z axis.
            0.0f                                  // z: always 0.0
        ));
        ssaoNoise.push_back(noise);
    }

    // 4x4 texture of random rotation vectors
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}

// The SSAO shader runs on a 2D screen-filled quad that calculates the occlusion value for each of its fragments.
// As we need to store the result of the SSAO stage (for use in the final lighting shader)
void SSAO::generateFramebuffers() {
    int w = getSSAOWidth();
    int h = getSSAOHeight();

    glGenFramebuffers(1, &ssaoFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);

    glGenTextures(1, &ssaoBuffer);
    glBindTexture(GL_TEXTURE_2D, ssaoBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, w, h, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);   // Linear for half-res upscaling
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); // Prevent blur edge artifacts
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoBuffer, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::SSAO::FRAMEBUFFER_NOT_COMPLETE" << std::endl;

    // Blur FBO - at full resolution for smooth upscaling
    glGenFramebuffers(1, &ssaoBlurFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glGenTextures(1, &ssaoBlurTexture);
    glBindTexture(GL_TEXTURE_2D, ssaoBlurTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_R16F, SCR_WIDTH, SCR_HEIGHT, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoBlurTexture, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::SSAO_BLUR::FRAMEBUFFER_NOT_COMPLETE" << std::endl;

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAO::renderSSAO(const GBuffer& gBuffer, const glm::mat4& projection) {
    // Rebuild FBOs if half-res setting changed
    if (resolutionChanged) {
        resolutionChanged = false;
        destroyFramebuffers();
        generateFramebuffers();
    }

    int w = getSSAOWidth();
    int h = getSSAOHeight();

    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    glViewport(0, 0, w, h);
    glClear(GL_COLOR_BUFFER_BIT);

    ssaoShader->use();

    // Only re-upload kernel samples when they change
    if (kernelDirty) {
        for (int i = 0; i < kernelSize; ++i) {
            ssaoShader->setVec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
        }
        ssaoShader->setInt("kernelSize", kernelSize);
        kernelDirty = false;
    }

    // Only re-upload projection when it changes
    if (projection != cachedProjection) {
        ssaoShader->setMat4("projection", projection);
        ssaoShader->setMat4("invProjection", glm::inverse(projection));
        cachedProjection = projection;
    }

    ssaoShader->setFloat("radius", radius);
    ssaoShader->setFloat("bias", bias);
    ssaoShader->setFloat("power", power);
    ssaoShader->setVec2("noiseScale", glm::vec2(
        static_cast<float>(w) / 4.0f,
        static_cast<float>(h) / 4.0f
    ));

    // Bind G-Buffer textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gBuffer.getDepthTexture());
    ssaoShader->setInt("gDepth", 0);

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, gBuffer.getNormalTexture());
    ssaoShader->setInt("gNormal", 1);

    glActiveTexture(GL_TEXTURE2);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    ssaoShader->setInt("texNoise", 2);

    // Draw fullscreen triangle
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void SSAO::blurSSAO() {
    // Blur FBO is always at full resolution - this upsamples half-res SSAO
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoBlurFBO);
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT);

    ssaoBlurShader->use();

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, ssaoBuffer);
    ssaoBlurShader->setInt("ssaoInput", 0);

    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

float SSAO::lerp(float a, float b, float f) {
    return a + f * (b - a);
}

void SSAO::resize(int w, int h) {
    if (w == SCR_WIDTH && h == SCR_HEIGHT && !resolutionChanged) return;
    SCR_WIDTH = w;
    SCR_HEIGHT = h;
    resolutionChanged = false;
    destroyFramebuffers();
    generateFramebuffers();
}

void SSAO::destroyFramebuffers() {
    if (ssaoFBO) {
        glDeleteFramebuffers(1, &ssaoFBO);
        glDeleteTextures(1, &ssaoBuffer);
        ssaoFBO = ssaoBuffer = 0;
    }
    if (ssaoBlurFBO) {
        glDeleteFramebuffers(1, &ssaoBlurFBO);
        glDeleteTextures(1, &ssaoBlurTexture);
        ssaoBlurFBO = ssaoBlurTexture = 0;
    }
}
