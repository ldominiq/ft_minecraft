#include "SSAO.hpp"

SSAO::SSAO(int width, int height) : SCR_WIDTH(width), SCR_HEIGHT(height) {
    ssaoShader  = std::make_unique<Shader>("shaders/ssao.vert", "shaders/ssao.frag");
    // ssaoBlurShader = std::make_unique<Shader>("shaders/ssao.vert", "shaders/ssaoBlur.frag");

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
    

    for (unsigned int i = 0; i < 64; ++i) {
        glm::vec3 sample(
            randomFloats(generator) * 2.0 - 1.0, // x: random float in range [-1.0, 1.0]
            randomFloats(generator) * 2.0 - 1.0, // y: random float in range [-1.0, 1.0]
            randomFloats(generator)              // z: random float in range [ 0.0, 1.0]
        );
        sample = glm::normalize(sample);
        sample *= randomFloats(generator);

        float scale = (float)i / 64.0;
        scale = lerp(0.1f, 1.0f, scale * scale);
        sample *= scale;

        ssaoKernel.push_back(sample);
    }
}

// By introducing some randomness onto the sample kernels we largely reduce the number of samples necessary to get good results.
void SSAO::generateNoiseTexture() {

    std::uniform_real_distribution<float> randomFloats(0.0, 1.0); // random floats between [0.0, 1.0]
    std::default_random_engine generator;
    std::vector<glm::vec3> ssaoNoise;

    for (unsigned int i = 0; i < 16; i++) {
        glm::vec3 noise (
            randomFloats(generator) * 2.0 - 1.0, // x: random float in range [-1.0, 1.0]
            randomFloats(generator) * 2.0 - 1.0, // y: random float in range [-1.0, 1.0]
            // As the sample kernel is oriented along the positive z direction in tangent space, we leave the z component at 0.0 so we rotate around the z axis. 
            0.0f                                 // z: always 0.0
        );
        ssaoNoise.push_back(noise);
    }

    // We then create a 4x4 texture that holds the random rotation vectors
    glGenTextures(1, &noiseTexture);
    glBindTexture(GL_TEXTURE_2D, noiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGB, GL_FLOAT, &ssaoNoise[0]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT); 
}

// The SSAO shader runs on a 2D screen-filled quad that calculates the occlusion value for each of its fragments.
// As we need to store the result of the SSAO stage (for use in the final lighting shader)
void SSAO::generateFramebuffers() {
    glGenFramebuffers(1, &ssaoFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);

    glGenTextures(1, &ssaoColorBuffer);
    glBindTexture(GL_TEXTURE_2D, ssaoColorBuffer);
    // As the ambient occlusion result is a single grayscale value we'll only need a texture's red component, so we set the color buffer's internal format to GL_RED. 
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, SCR_WIDTH, SCR_HEIGHT, 0, GL_RED, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, ssaoColorBuffer, 0);
    
}

void SSAO::renderSSAO(const GBuffer& gBuffer, const glm::mat4& projection) {
    glBindFramebuffer(GL_FRAMEBUFFER, ssaoFBO);
    glViewport(0, 0, SCR_WIDTH, SCR_HEIGHT);
    glClear(GL_COLOR_BUFFER_BIT);

    ssaoShader->use();

    // Upload kernel samples
    for (int i = 0; i < kernelSize; ++i) {
        ssaoShader->setVec3("samples[" + std::to_string(i) + "]", ssaoKernel[i]);
    }
    ssaoShader->setInt("kernelSize", kernelSize);
    ssaoShader->setMat4("projection", projection);
    ssaoShader->setFloat("radius", radius);
    ssaoShader->setFloat("bias", bias);
    ssaoShader->setVec2("noiseScale", glm::vec2(
        static_cast<float>(SCR_WIDTH) / 4.0f,
        static_cast<float>(SCR_HEIGHT) / 4.0f
    ));

    // Bind G-Buffer textures
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gBuffer.getPositionTexture());
    ssaoShader->setInt("gPosition", 0);

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

float SSAO::lerp(float a, float b, float f) {
    return a + f * (b - a);
}

void SSAO::resize(int w, int h) {
    if (w == SCR_WIDTH && h == SCR_HEIGHT) return;
    SCR_WIDTH = w;
    SCR_HEIGHT = h;
    destroyFramebuffers();
    generateFramebuffers();
}

void SSAO::destroyFramebuffers() {
    if (ssaoFBO) {
        glDeleteFramebuffers(1, &ssaoFBO);
        glDeleteTextures(1, &ssaoColorBuffer);
        ssaoFBO = ssaoColorBuffer = 0;
    }
    // if (ssaoBlurFBO) {
    //     glDeleteFramebuffers(1, &ssaoBlurFBO);
    //     glDeleteTextures(1, &ssaoBlurTexture);
    //     ssaoBlurFBO = ssaoBlurTexture = 0;
    // }
}
