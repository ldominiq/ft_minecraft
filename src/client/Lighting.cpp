#include "Lighting.hpp"

Lighting::Lighting(int screenWidth, int screenHeight) : width(screenWidth), height(screenHeight) {
    // VAO for fullscreen triangle (no attributes needed)
    glGenVertexArrays(1, &skyVAO);
    glBindVertexArray(skyVAO);
    glBindVertexArray(0);

    skyShader = std::make_unique<Shader>("shaders/sky.vert", "shaders/sky.frag");
    lightCubeShader = std::make_unique<Shader>("shaders/lightCubeShader.vert", "shaders/lightCubeShader.frag");

    // Light cube setup
    glGenVertexArrays(1, &lightCubeVAO);
    glGenBuffers(1, &lightCubeVBO);
    glBindVertexArray(lightCubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lightCubeVBO);
    

    glBufferData(GL_ARRAY_BUFFER, sizeof(lightCubeVertices), lightCubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

Lighting::~Lighting() {
    glDeleteVertexArrays(1, &skyVAO);
    glDeleteVertexArrays(1, &lightCubeVAO);
    glDeleteBuffers(1, &lightCubeVBO);
}

void Lighting::drawSky(const glm::mat4& view, const glm::mat4& projection, glm::vec3 cameraPos) const {
    skyShader->use();

    skyShader->setVec2("resolution", glm::vec2(width, height));
    skyShader->setFloat("time", skyTimeOffset);
    skyShader->setMat4("view", view);
    skyShader->setMat4("projection", projection);
    skyShader->setVec3("cameraPosWorld", cameraPos);
    skyShader->setFloat("seaLevel", 64.0f);
    skyShader->setFloat("exposure", skyExposure);
    skyShader->setFloat("atmDensity", skyAtmDensity);
    skyShader->setFloat("atmThickness", skyAtmThickness);
    skyShader->setFloat("planetScale", planetScale);
    skyShader->setVec3("sunDir", getDirectionalLightDirection());

    // Disable depth test and writes for background
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glBindVertexArray(skyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void Lighting::drawLightCubes(const glm::mat4& view, const glm::mat4& projection) const {
    lightCubeShader->use();
    // we now draw as many light bulbs as we have point lights.
    glBindVertexArray(lightCubeVAO);
    for (unsigned int i = 0; i < 3; i++)
    {
        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, pointLightPositions[i]);
        model = glm::scale(model, glm::vec3(0.2f)); // Make it a smaller cube
        // Set per-cube color here so each light uses its own color
        glm::vec3 cubeCol = pointLightsOn[i] ? pointLightDiffuse[i] : glm::vec3(0.0f);
        lightCubeShader->setVec3("cubeColor", cubeCol);
        lightCubeShader->setMat4("model", model);
        lightCubeShader->setMat4("projection", projection);
        lightCubeShader->setMat4("view", view);
        glDrawArrays(GL_TRIANGLES, 0, 36);
    }
}

void Lighting::updateSunDirection(float deltaTime) {
    // Time management for sky shader
    if (skyTimePaused == false)
        skyTimeOffset += deltaTime * 0.05f; // Speed of sun movement

    const float timeScale = 0.1f;
    const float t = skyTimeOffset * timeScale;

    glm::vec3 sunDirLocal = glm::normalize(glm::vec3(
        std::sin(t), // x (azimuth)
        std::cos(t), // y (elevation)
        0.0f));     // z

    // Sun direction based on time of day
    // Sun moves in a circle in the sky, with yaw adjustment
    float sunYawRad = glm::radians(sunYawDeg);
    glm::vec3 sunDir = glm::normalize(glm::vec3(
        sunDirLocal.x * std::cos(sunYawRad) - sunDirLocal.z * std::sin(sunYawRad),
        sunDirLocal.y,
        sunDirLocal.x * std::sin(sunYawRad) + sunDirLocal.z * std::cos(sunYawRad)
    ));
    directionalLightDir = sunDir;
}


// Getters
bool Lighting::isPointLightOn(int index) const { 
    if (index < 0 || index >= 3) return false;
    return pointLightsOn[index];
};
glm::vec3 Lighting::getPointLightPosition(int index) const { 
    if (index < 0 || index >= 3) return glm::vec3(0.0f);
    return pointLightPositions[index]; 
};
glm::vec3 Lighting::getPointLightAmbient(int index) const { 
    if (index < 0 || index >= 3) return glm::vec3(0.0f);
    return pointLightAmbient[index]; 
};
glm::vec3 Lighting::getPointLightDiffuse(int index) const { 
    if (index < 0 || index >= 3) return glm::vec3(0.0f);
    return pointLightDiffuse[index]; 
};
glm::vec3 Lighting::getPointLightSpecular(int index) const { 
    if (index < 0 || index >= 3) return glm::vec3(0.0f);
    return pointLightSpecular[index]; 
};
float Lighting::getPointLightConstant(int index) const { 
    if (index < 0 || index >= 3) return 1.0f;
    return pointLightConstant[index]; 
};
float Lighting::getPointLightLinear(int index) const { 
    if (index < 0 || index >= 3) return 0.0f;
    return pointLightLinear[index]; 
};
float Lighting::getPointLightQuadratic(int index) const { 
    if (index < 0 || index >= 3) return 0.0f;
    return pointLightQuadratic[index]; 
};
