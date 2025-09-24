#include "Lighting.hpp"

Lighting::Lighting(int screenWidth, int screenHeight) : width(screenWidth), height(screenHeight) {
    // VAO for fullscreen triangle (no attributes needed)
    glGenVertexArrays(1, &skyVAO);
    glBindVertexArray(skyVAO);
    glBindVertexArray(0);

    skyShader = std::make_unique<Shader>("shaders/sky.vert", "shaders/sky.frag");
    lightCubeShader = std::make_unique<Shader>("shaders/lightCubeShader.vert", "shaders/lightCubeShader.frag");
    simpleDepthShader = std::make_shared<Shader>("shaders/shadowDepthShader.vert", "shaders/shadowDepthShader.frag");
    debugDepthQuad = std::make_shared<Shader>("shaders/shadowDebugShader.vert", "shaders/shadowDebugShader.frag");


    // Light cube setup
    glGenVertexArrays(1, &lightCubeVAO);
    glGenBuffers(1, &lightCubeVBO);
    glBindVertexArray(lightCubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lightCubeVBO);
    

    glBufferData(GL_ARRAY_BUFFER, sizeof(lightCubeVertices), lightCubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
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
        auto model = glm::mat4(1.0f);
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

void Lighting::updateSunDirection(const float deltaTime) {
    // Time management for sky shader
    if (skyTimePaused == false)
        skyTimeOffset += deltaTime * 0.05f; // Speed of sun movement

    constexpr float timeScale = 0.1f;
    const float t = skyTimeOffset * timeScale;

    const glm::vec3 sunDirLocal = glm::normalize(glm::vec3(
        std::sin(t), // x (azimuth)
        std::cos(t), // y (elevation)
        0.0f));     // z

    // Sun direction based on time of day
    // Sun moves in a circle in the sky, with yaw adjustment
    const float sunYawRad = glm::radians(sunYawDeg);
    const glm::vec3 sunDir = glm::normalize(glm::vec3(
        sunDirLocal.x * std::cos(sunYawRad) - sunDirLocal.z * std::sin(sunYawRad),
        sunDirLocal.y,
        sunDirLocal.x * std::sin(sunYawRad) + sunDirLocal.z * std::cos(sunYawRad)
    ));
    directionalLightDir = sunDir;
}

void Lighting::uploadLightingUniforms(const Shader &shader, const glm::vec3 &cameraPos, const glm::vec3 cameraFront) const {
    // 2. Render the scene normally, using the generated shadow map to determine shadowed fragments.
    // The following code implements both steps each frame.
    shader.use();

    // set light uniforms
    shader.setVec3("viewPos", cameraPos);
    shader.setVec3("lightPos", lightPos);
    shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    // shader.setInt("shadows", shadowsEnabled ? 1 : 0); // enable/disable shadows by pressing 'SPACE'
    shader.setFloat("shadows.MIN_BIAS", MIN_BIAS);
    shader.setFloat("shadows.MAX_BIAS", MAX_BIAS);
    shader.setInt("shadows.PCF_RADIUS", PCF_RADIUS);
    shader.setInt("shadows.POISSON_SAMPLES", POISSON_SAMPLES);
    shader.setFloat("shadows.POISSON_RADIUS_BASE", POISSON_RADIUS_BASE);
    shader.setFloat("shadows.POISSON_RADIUS_SCALE", POISSON_RADIUS_SCALE);
    // shader.setFloat("shadows.AMBIENT_OCCLUSION", ambientOcclusion);
    shader.setFloat("shadows.CONTACT_OFFSET", shadowContactOffset);
    shader.setFloat("shadows.enabled", shadowsEnabled);

    // Lighting uniforms
    // ====================================


    shader.setFloat("material.shininess", materialShininess);

    // directional light
    if (directionalLightOn) {

        // day/night factror based on sun elevation
        float day = glm::clamp(-cachedShadowLightDir.y * 2.0f, 0.0f, 1.0f);
        // smooth transition near sunset/sunrise
        day = glm::smoothstep(0.0f, 1.0f, day);

        // small ambiant light at night
        constexpr float nightAmbientMin = 0.3f;
        const glm::vec3 ambientColor = directionalAmbientColor * (nightAmbientMin + (1.0f - nightAmbientMin) * day);
        const glm::vec3 diffuseColor = directionalDiffuseColor * day;
        const glm::vec3 specularColor = directionalSpecularColor * day;
        shader.setVec3("dirLight.direction", cachedShadowLightDir);
        shader.setVec3("dirLight.ambient", ambientColor);
        shader.setVec3("dirLight.diffuse", diffuseColor);
        shader.setVec3("dirLight.specular", specularColor);
    } else {
        shader.setVec3("dirLight.ambient", glm::vec3(0.0f));
        shader.setVec3("dirLight.diffuse", glm::vec3(0.0f));
        shader.setVec3("dirLight.specular", glm::vec3(0.0f));
    }
    // point lights
    for ( int i=0; i < 3; i++ ) {
        if (!pointLightsOn[i]) {
            shader.setVec3("pointLights[" + std::to_string(i) + "].ambient", glm::vec3(0.0f));
            shader.setVec3("pointLights[" + std::to_string(i) + "].diffuse", glm::vec3(0.0f));
            shader.setVec3("pointLights[" + std::to_string(i) + "].specular", glm::vec3(0.0f));
            continue;
        }
        shader.setVec3("pointLights[" + std::to_string(i) + "].position", pointLightPositions[i]);
        shader.setVec3("pointLights[" + std::to_string(i) + "].ambient", pointLightAmbient[i]);
        shader.setVec3("pointLights[" + std::to_string(i) + "].diffuse", pointLightDiffuse[i]);
        shader.setVec3("pointLights[" + std::to_string(i) + "].specular", pointLightSpecular[i]);
        shader.setFloat("pointLights[" + std::to_string(i) + "].constant", pointLightConstant[i]);
        shader.setFloat("pointLights[" + std::to_string(i) + "].linear", pointLightLinear[i]);
        shader.setFloat("pointLights[" + std::to_string(i) + "].quadratic", pointLightQuadratic[i]);
    }
    // spotLight (flashlight)
    if (flashlightOn) {
        shader.setVec3("spotLight.position", cameraPos);
        shader.setVec3("spotLight.direction", cameraFront);
        shader.setVec3("spotLight.ambient", glm::vec3(0.0f));
        shader.setVec3("spotLight.diffuse", glm::vec3(1.0f));
        shader.setVec3("spotLight.specular", glm::vec3(1.0f));
        shader.setFloat("spotLight.constant", spotLightConstant);
        shader.setFloat("spotLight.linear", spotLightLinear);
        shader.setFloat("spotLight.quadratic", spotLightQuadratic);
        shader.setFloat("spotLight.cutOff", glm::cos(glm::radians(flashlightCutoff)));
        shader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(flashlightOuterCutoff)));
    } else {
        shader.setVec3("spotLight.position", cameraPos);
        shader.setVec3("spotLight.direction", cameraFront);
        shader.setVec3("spotLight.ambient", glm::vec3(0.0f));
        shader.setVec3("spotLight.diffuse", glm::vec3(0.0f));
        shader.setVec3("spotLight.specular", glm::vec3(0.0f));
        shader.setFloat("spotLight.constant", spotLightConstant);
        shader.setFloat("spotLight.linear", spotLightLinear);
        shader.setFloat("spotLight.quadratic", spotLightQuadratic);
        shader.setFloat("spotLight.cutOff", glm::cos(glm::radians(flashlightCutoff)));
        shader.setFloat("spotLight.outerCutOff", glm::cos(glm::radians(flashlightOuterCutoff)));
    }

    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, depthMap);
}

void Lighting::updateShadowMap(const Renderer& renderer, const glm::vec3& cameraPos) {
    // Shadow mapping
    // ====================================
    // 1. Render the depth of the scene to a texture from the light's perspective.
    //    This generates a shadow map, which will be sampled in the main render pass
    //    to determine which fragments are in shadow and apply realistic lighting.
    // --------------------------------------------------------------

    const float orthoRange = shadowOrthoRange; // how far from center to render shadows

    const bool doUpdate = forceShadowUpdate || (shadowFrameCounter % shadowUpdateInterval) == 0;

    // If shadow quality changed, update shadow resolution and re-create depth texture/FBO
    static ShadowQuality lastShadowQuality = shadowQuality;
    if (lastShadowQuality != shadowQuality) {
        lastShadowQuality = shadowQuality;
        refreshShadowResolution();

        glDeleteTextures(1, &depthMap);
        glDeleteFramebuffers(1, &depthMapFBO);

        glGenFramebuffers(1, &depthMapFBO);
        glGenTextures(1, &depthMap);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        float borderColor[] = {1.0f,1.0f,1.0f,1.0f};
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    if (doUpdate) {
        forceShadowUpdate = false;
        cachedShadowLightDir = -directionalLightDir;


        // Center the shadow (orthographic) frustum around the player instead of world origin
        const glm::vec3 center = cameraPos;


        setLightPos(center - cachedShadowLightDir * 200.0f);
        lightView = glm::lookAt(lightPos, center, glm::vec3(0.0f, 1.0f, 0.0f));

        // Ortho volume still symmetric, but now relative to player-centered lightView
        lightProjection = glm::ortho(-orthoRange, orthoRange,
                                     -orthoRange, orthoRange,
                                     shadowNearPlane,  shadowFarPlane);

        lightSpaceMatrix = lightProjection * lightView;

        // render scene from light's point of view
        simpleDepthShader->use();
        simpleDepthShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);

        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);

        glCullFace(GL_FRONT); // required so shadows don't bug through mountains

        renderer.render(simpleDepthShader);
        // floor
        constexpr auto model = glm::mat4(1.0f);
        simpleDepthShader->setMat4("model", model);
        glBindVertexArray(planeVAO);
        glDrawArrays(GL_TRIANGLES, 0, 6);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        glCullFace(GL_BACK);

        // Immediately restore viewport after unbinding framebuffer
        glViewport(0, 0, width, height);
    }
    shadowFrameCounter++;
}

void Lighting::refreshShadowResolution() {
    switch (shadowQuality) {
        case ShadowQuality::Low:    SHADOW_WIDTH = 1024;  SHADOW_HEIGHT = 1024;  break;
        case ShadowQuality::Medium: SHADOW_WIDTH = 2048;  SHADOW_HEIGHT = 2048;  break;
        case ShadowQuality::High:   SHADOW_WIDTH = 4096;  SHADOW_HEIGHT = 4096;  break;
        case ShadowQuality::Ultra:  SHADOW_WIDTH = 8192;  SHADOW_HEIGHT = 8192;  break;
        default:                    SHADOW_WIDTH = 4096;  SHADOW_HEIGHT = 4096;  break;
    }
}

void Lighting::initShadowGroundPlane() {
    // plane VAO
    unsigned int planeVBO;
    glGenVertexArrays(1, &planeVAO);
    glGenBuffers(1, &planeVBO);
    glBindVertexArray(planeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, planeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(planeVertices), planeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), static_cast<void *>(nullptr));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void *>(3 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(float), reinterpret_cast<void *>(6 * sizeof(float)));
    glBindVertexArray(0);
}


void Lighting::initShadowResources() {
    refreshShadowResolution(); // Ensure shadowWidth/shadowHeight are set according to shadowQuality
    glGenFramebuffers(1, &depthMapFBO);
    // create depth texture
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    // Clamp to border to avoid shadow edge sampling artifacts
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    constexpr float borderColor[] = {1.0f,1.0f,1.0f,1.0f};
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    // attach depth texture as FBO's depth buffer
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Lighting::drawShadowMapPreview() {
    debugDepthQuad->use();
    debugDepthQuad->setFloat("near_plane", shadowNearPlane);
    debugDepthQuad->setFloat("far_plane", shadowFarPlane);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depthMap);


    drawTexturePreviewQuad(depthMap);
}

void Lighting::initShadowDebugShader() const {
    debugDepthQuad->use();
    debugDepthQuad->setInt("depthMap", 0);
}

// Draws a small textured quad (preview of an FBO texture) in the top-right corner.
void Lighting::drawTexturePreviewQuad(unsigned int textureID) {
    if (textureID == 0) return;

    static std::shared_ptr<Shader> debugFBOShader;
    static GLuint vao = 0;
    static GLuint vbo = 0;

    if (!debugFBOShader) {
        debugFBOShader = std::make_shared<Shader>(
            "shaders/debugRenderer.vert",
            "shaders/debugRenderer.frag"
        );
        debugFBOShader->use();
        debugFBOShader->setInt("texCoords", 0);
    }

    if (vao == 0) {
        // NDC quad in top-right corner
        float verts[] = {
            //  pos.xy    uv
            0.40f, 0.90f, 1.0f, 1.0f,
            0.40f, 0.40f, 1.0f, 0.0f,
            0.90f, 0.40f, 0.0f, 0.0f,

            0.40f, 0.90f, 1.0f, 1.0f,
            0.90f, 0.40f, 0.0f, 0.0f,
            0.90f, 0.90f, 0.0f, 1.0f
       };
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); // position
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(1); // uv
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glBindVertexArray(0);
    }

    GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthEnabled) glDisable(GL_DEPTH_TEST);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    debugFBOShader->use();
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    if (depthEnabled) glEnable(GL_DEPTH_TEST);
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
