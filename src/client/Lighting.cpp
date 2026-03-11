#include "Lighting.hpp"

Lighting::Lighting(const int screenWidth, const int screenHeight) : width(screenWidth), height(screenHeight) {
    // VAO for fullscreen triangle (no attributes needed)
    glGenVertexArrays(1, &skyVAO);
    glBindVertexArray(skyVAO);
    glBindVertexArray(0);

    glGenVertexArrays(1, &cloudsVAO);
    glBindVertexArray(cloudsVAO);
    glBindVertexArray(0);

    skyShader = std::make_unique<Shader>("shaders/sky.vert", "shaders/sky.frag");
    skyLUTRenderShader = std::make_unique<Shader>("shaders/sky.vert", "shaders/skyLUT_render.frag");
    lightCubeShader = std::make_unique<Shader>("shaders/lightCubeShader.vert", "shaders/lightCubeShader.frag");
    cloudShader = std::make_shared<Shader>("shaders/clouds.vert", "shaders/clouds.frag");

    skyLUT = std::make_unique<SkyLUT>(256, 128);

    cloudFBO = std::make_unique<CloudFramebuffer>(width , height, cloudDownscale);

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
    if (glfwGetCurrentContext()) {
        glDeleteBuffers(1, &lightCubeVBO);
        glDeleteBuffers(1, &debugVBO);

        glDeleteVertexArrays(1, &lightCubeVAO);
        glDeleteVertexArrays(1, &skyVAO);
        glDeleteVertexArrays(1, &planeVAO);
        glDeleteVertexArrays(1, &debugVAO);

        glDeleteVertexArrays(1, &cloudsVAO);

        glDeleteTextures(1, &csmDepthMaps);
        glDeleteFramebuffers(1, &csmFBO);

    } else {
        lightCubeVAO = 0;
        lightCubeVBO = 0;
        skyVAO = 0;
        planeVAO = 0;
        debugVAO = 0;
        debugVBO = 0;
        cloudFBO = nullptr;
        csmDepthMaps = 0;
        csmFBO = 0;
    }
}

GLuint Lighting::getCloudTexture() const
{
    return cloudFBO ? cloudFBO->getColorTexture() : 0;
}

void Lighting::renderCloudsLowRes(const glm::mat4& view, const glm::mat4& projection, const glm::vec3& cameraPos) const
{
    if (!cloudsEnabled || !cloudFBO || !cloudShader)
        return;

    cloudFBO->bind();
    glViewport(0, 0, cloudFBO->getWidth(), cloudFBO->getHeight());
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);

    // Clear to "no cloud": rgb=0, transmittance=1 (alpha=1)
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    cloudShader->use();

    // Keep uniforms consistent with your sky shader usage
    cloudShader->setVec2("resolution", glm::vec2(cloudFBO->getWidth(), cloudFBO->getHeight()));
    cloudShader->setFloat("time", skyTimeOffset);
    cloudShader->setMat4("view", view);
    cloudShader->setMat4("projection", projection);
    cloudShader->setVec3("cameraPosWorld", cameraPos);
    cloudShader->setVec3("sunDir", getDirectionalLightDirection());

    // Cloud box follows camera for infinite clouds
    // Keep clouds at fixed altitude but extend horizontally around camera
    const float cloudRadius = 500.0f;  // Horizontal extent around camera
    const float cloudMinY = 260.0f;     // Bottom of cloud layer
    const float cloudMaxY = 310.0f;     // Top of cloud layer
    const glm::vec3 bmin(cameraPos.x - cloudRadius, cloudMinY, cameraPos.z - cloudRadius);
    const glm::vec3 bmax(cameraPos.x + cloudRadius, cloudMaxY, cameraPos.z + cloudRadius);
    cloudShader->setVec3("cloudBoxMinWorld", bmin);
    cloudShader->setVec3("cloudBoxMaxWorld", bmax);

    cloudShader->setFloat("cloudDensity", cloudDensity);
    cloudShader->setFloat("cloudSigmaT", cloudSigmaT);
    cloudShader->setVec3("cloudAlbedo", cloudAlbedo);
    cloudShader->setFloat("cloudStepCount", cloudStepCount);

    cloudShader->setFloat("cloudSigmaS", cloudSigmaS);
    cloudShader->setFloat("cloudPhaseG", cloudPhaseG);

    // Modulate ambient by sun elevation (darker at night)
    glm::vec3 sunDirNorm = glm::normalize(getDirectionalLightDirection());
    float sunElevation = sunDirNorm.y;  // Can be negative (below horizon)
    float dayFactor = glm::smoothstep(-0.2f, 0.1f, sunElevation);  // Fade from -0.2 to 0.1
    float nightAmbient = 0.01f;  // Very low ambient at night
    float dayAmbient = 0.5f;     // Full ambient during day
    float ambientStrength = glm::mix(nightAmbient, dayAmbient, dayFactor);

    cloudShader->setVec3("cloudAmbientColor", glm::vec3(0.65f, 0.72f, 0.85f));
    cloudShader->setFloat("cloudAmbientStrength", ambientStrength);

    cloudShader->setVec3("cloudSunColor", glm::vec3(1.0f, 0.98f, 0.95f));
    cloudShader->setFloat("cloudSunStrength", 25.0f);  // Increased from 15.0f for brighter clouds

    // TODO: add params to imgui
    cloudShader->setFloat("cloudEdgeFeather", cloudEdgeFeather);
    cloudShader->setFloat("cloudNoiseScale", cloudNoiseScale);
    cloudShader->setFloat("cloudNoiseContrastLo", cloudNoiseContrastLo);
    cloudShader->setFloat("cloudNoiseContrastHi", cloudNoiseContrastHi);
    cloudShader->setFloat("cloudWindSpeed", cloudWindSpeed);
    cloudShader->setVec2("cloudWindDir", cloudWindDir);

    glBindVertexArray(cloudsVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);

    CloudFramebuffer::unbind();

    // Restore default viewport for subsequent passes
    glViewport(0, 0, width, height);
}

void Lighting::updateSkyLUT(float cameraPosY) {
    if (skyLUTEnabled && skyLUT) {
        skyLUT->update(skyAtmDensity, skyAtmThickness, cameraPosY, seaLevel, planetScale, width, height);
    }
}

void Lighting::drawSky(const glm::mat4& view, const glm::mat4& projection, glm::vec3 cameraPos) const {
    // Choose shader: LUT-based (fast) or full ray-marching (reference)
    const bool useLUT = skyLUTEnabled && skyLUT && skyLUT->getLUTTexture();
    Shader* shader = useLUT ? skyLUTRenderShader.get() : skyShader.get();

    shader->use();

    shader->setVec2("resolution", glm::vec2(width, height));
    shader->setMat4("view", view);
    shader->setMat4("projection", projection);
    shader->setVec3("cameraPosWorld", cameraPos);
    shader->setFloat("exposure", skyExposure);
    shader->setVec3("sunDir", getDirectionalLightDirection());

    if (useLUT) {
        // Bind the precomputed scattering LUT
        glActiveTexture(GL_TEXTURE9);
        glBindTexture(GL_TEXTURE_2D, skyLUT->getLUTTexture());
        shader->setInt("skyLUT", 9);
    } else {
        // Full ray-marching path needs these extra uniforms
        shader->setFloat("seaLevel", seaLevel);
        shader->setFloat("planetScale", planetScale);
        shader->setFloat("time", skyTimeOffset);
        shader->setFloat("atmDensity", skyAtmDensity);
        shader->setFloat("atmThickness", skyAtmThickness);
    }

    // Cloud composite
    const bool composite = cloudsEnabled && (getCloudTexture() != 0);
    shader->setInt("cloudsCompositeEnabled", composite ? 1 : 0);

    if (composite) {
        glActiveTexture(GL_TEXTURE8);
        glBindTexture(GL_TEXTURE_2D, getCloudTexture());
        shader->setInt("cloudTex", 8);
    }

    // Render sky with depth = far plane, terrain will render in front
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);    // Write depth to allow terrain occlusion
    glBindVertexArray(skyVAO);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);    // Restore default
    glDepthMask(GL_TRUE);
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
    // ── "Skyrim approach" ──────────────────────────────────────────────
    // The sun holds perfectly still for sunPauseDuration seconds, then
    // smoothly advances over sunStepDuration seconds.
    constexpr float sunSpeed = 0.05f;

    if (!skyTimePaused) {
        if (!sunStepping) {
            // ── HOLD phase ──
            sunPauseTimer += deltaTime;
            if (sunPauseTimer >= sunPauseDuration) {
                // Switch to stepping
                sunStepping  = true;
                sunStepTimer = 0.0f;
            }
        } else {
            // ── STEP phase: advance skyTimeOffset smoothly ──
            sunStepTimer += deltaTime;
            float t_step = glm::clamp(sunStepTimer / sunStepDuration, 0.0f, 1.0f);
            // Use smoothstep to ease in/out so the jump isn't jarring
            float smoothT = glm::smoothstep(0.0f, 1.0f, t_step);

            // Total offset this step must cover = what would've accumulated
            // during the whole pause+step cycle at the original speed.
            float totalCycleDuration = sunPauseDuration + sunStepDuration;
            float totalStepOffset    = totalCycleDuration * sunSpeed;

            // Derivative of smoothstep gives the per-frame advance
            // We compute the current position as base + smoothT * totalStepOffset
            // and store the base at the start of the step.
            // Simpler: just set skyTimeOffset = stepBase + smoothT * totalStepOffset
            // We store the base in sunPauseTimer (repurposed during step).
            if (sunStepTimer <= deltaTime) {
                // First frame of the step: store the base offset
                sunPauseTimer = skyTimeOffset; // repurpose as stepBase
            }
            skyTimeOffset = sunPauseTimer + smoothT * totalStepOffset;

            if (t_step >= 1.0f) {
                // Step complete — back to hold
                sunStepping   = false;
                sunPauseTimer = 0.0f;
                sunStepTimer  = 0.0f;
            }
        }
    }

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
    // Always keep cachedShadowLightDir in sync so the day/night factor
    // and shader direction are correct even when shadow rendering is skipped.
    cachedShadowLightDir = -directionalLightDir;
}

void Lighting::uploadLightingUniforms(const Shader &shader, const glm::vec3 &cameraPos, const glm::vec3 cameraFront) const {
    // 2. Render the scene normally, using the generated shadow map to determine shadowed fragments.
    // The following code implements both steps each frame.
    shader.use();

    // set light uniforms
    shader.setVec3("viewPos", cameraPos);
    shader.setVec3("lightPos", lightPos);
    shader.setMat4("lightSpaceMatrix", lightSpaceMatrix);
    shader.setFloat("shadows.MIN_BIAS", MIN_BIAS);
    shader.setFloat("shadows.MAX_BIAS", MAX_BIAS);
    shader.setFloat("shadows.enabled", shadowsEnabled);

    // CSM depth maps are bound separately in uploadCSMUniforms()

    // Lighting uniforms
    // ====================================


    shader.setFloat("material.shininess", materialShininess);

    // directional light
    if (directionalLightOn) {

        // day/night factor based on sun elevation
        // directionalLightDir.y > 0 means sun above horizon
        float sunElevation = directionalLightDir.y;
        float day = glm::clamp(sunElevation * 2.0f, 0.0f, 1.0f);
        // smooth transition near sunset/sunrise
        day = glm::smoothstep(0.0f, 1.0f, day);

        // small ambient light at night
        constexpr float nightAmbientMin = 0.3f;
        const glm::vec3 ambientColor = directionalAmbientColor * (nightAmbientMin + (1.0f - nightAmbientMin) * day);
        const glm::vec3 diffuseColor = directionalDiffuseColor * day;
        const glm::vec3 specularColor = directionalSpecularColor * day;
        shader.setVec3("dirLight.direction", -directionalLightDir);
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
}

void Lighting::drawCSMShadowMapPreview(int cascadeLayer)
{
    GLuint layerView;
    glGenTextures(1, &layerView);
    glTextureView(layerView, GL_TEXTURE_2D, csmDepthMaps,
                  GL_DEPTH_COMPONENT32F,
                  0, 1,           // mip levels
                  cascadeLayer, 1); // one layer

    // Override to GL_NONE so the preview quad reads raw depth.
    glBindTexture(GL_TEXTURE_2D, layerView);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
    
    drawTexturePreviewQuad(layerView);
    
    glDeleteTextures(1, &layerView);
}

void Lighting::drawCSMDebugView(const glm::vec3& cameraPos, const glm::vec3& cameraFront, const glm::mat4& cameraView)
{
    const int numCascades = static_cast<int>(shadowCascadeLevels.size()) + 1;

    // Persistent state across frames
    static float zoomLevel = 1.0f;
    static bool showLightFrustums = true;
    static bool showCameraFrustums = true;
    static bool showGrid = true;
    static bool frozen = false;
    static glm::vec3 frozenCameraPos;
    static glm::vec3 frozenCameraFront;
    static glm::mat4 frozenCameraView;

    // When freezing, capture current state; when unfreezing, use live data
    const glm::vec3& drawPos   = frozen ? frozenCameraPos   : cameraPos;
    const glm::vec3& drawFront = frozen ? frozenCameraFront : cameraFront;
    const glm::mat4& drawView  = frozen ? frozenCameraView  : cameraView;

    ImGui::SetNextWindowSize(ImVec2(420, 520), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(10, 10), ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("CSM Cascade Radar", &showCSMDebugView)) {
        ImGui::End();
        return;
    }

    // --- Controls ---
    if (ImGui::Button(frozen ? "Unfreeze" : "Freeze")) {
        frozen = !frozen;
        if (frozen) {
            frozenCameraPos   = cameraPos;
            frozenCameraFront = cameraFront;
            frozenCameraView  = cameraView;
        }
    }
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &showGrid);
    ImGui::SameLine();
    ImGui::Checkbox("Camera", &showCameraFrustums);
    ImGui::SameLine();
    ImGui::Checkbox("Light", &showLightFrustums);

    ImGui::SliderFloat("Zoom", &zoomLevel, 0.1f, 10.0f, "%.1fx");

    if (frozen) {
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "FROZEN - move camera to compare");
    }

    // --- Canvas setup ---
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    // Reserve space for the stats panel below
    float statsHeight = 20.0f * numCascades + 30.0f;
    float side = std::min(canvasSize.x, canvasSize.y - statsHeight);
    if (side < 80.0f) { ImGui::End(); return; }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center(canvasPos.x + side * 0.5f, canvasPos.y + side * 0.5f);
    float worldRadius = (cameraFarPlane / zoomLevel) * 1.1f;

    // Clip drawing to canvas
    drawList->PushClipRect(canvasPos, ImVec2(canvasPos.x + side, canvasPos.y + side), true);

    // Background
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + side, canvasPos.y + side),
                            IM_COL32(15, 15, 20, 240));

    // Helper: world XZ → screen pixel (north-up, centered on player)
    auto worldToRadar = [&](float wx, float wz) -> ImVec2 {
        float dx = wx - drawPos.x;
        float dz = wz - drawPos.z;
        float sx = center.x + (dx / worldRadius) * (side * 0.45f);
        float sy = center.y + (dz / worldRadius) * (side * 0.45f);
        return ImVec2(sx, sy);
    };

    // --- Grid ---
    if (showGrid) {
        // Pick a nice grid spacing based on zoom
        float gridSpacing = 50.0f;
        if (worldRadius > 400) gridSpacing = 100.0f;
        if (worldRadius > 800) gridSpacing = 200.0f;
        if (worldRadius < 100) gridSpacing = 25.0f;
        if (worldRadius < 50)  gridSpacing = 10.0f;

        float startW = floorf(drawPos.x / gridSpacing - worldRadius / gridSpacing) * gridSpacing;
        float endW   = ceilf(drawPos.x / gridSpacing + worldRadius / gridSpacing) * gridSpacing;

        for (float w = startW; w <= endW; w += gridSpacing) {
            ImVec2 a = worldToRadar(w, drawPos.z - worldRadius);
            ImVec2 b = worldToRadar(w, drawPos.z + worldRadius);
            drawList->AddLine(a, b, IM_COL32(50, 50, 50, 120), 1.0f);
        }
        startW = floorf(drawPos.z / gridSpacing - worldRadius / gridSpacing) * gridSpacing;
        endW   = ceilf(drawPos.z / gridSpacing + worldRadius / gridSpacing) * gridSpacing;
        for (float w = startW; w <= endW; w += gridSpacing) {
            ImVec2 a = worldToRadar(drawPos.x - worldRadius, w);
            ImVec2 b = worldToRadar(drawPos.x + worldRadius, w);
            drawList->AddLine(a, b, IM_COL32(50, 50, 50, 120), 1.0f);
        }

        // Scale label
        ImVec2 scaleStart = worldToRadar(drawPos.x - gridSpacing * 0.5f, drawPos.z + worldRadius * 0.85f);
        ImVec2 scaleEnd   = worldToRadar(drawPos.x + gridSpacing * 0.5f, drawPos.z + worldRadius * 0.85f);
        drawList->AddLine(scaleStart, scaleEnd, IM_COL32(150, 150, 150, 180), 2.0f);
        char scaleTxt[32];
        snprintf(scaleTxt, sizeof(scaleTxt), "%.0f blocks", gridSpacing);
        drawList->AddText(ImVec2((scaleStart.x + scaleEnd.x) * 0.5f - 25, scaleStart.y + 3),
                          IM_COL32(150, 150, 150, 180), scaleTxt);
    }

    // --- Cascade colors ---
    const ImU32 cascadeFills[] = {
        IM_COL32(255, 80,  80,  35),
        IM_COL32(80,  255, 80,  35),
        IM_COL32(80,  80,  255, 35),
        IM_COL32(255, 255, 80,  35),
        IM_COL32(255, 80,  255, 35),
    };
    const ImU32 cascadeOutlines[] = {
        IM_COL32(255, 100, 100, 220),
        IM_COL32(100, 255, 100, 220),
        IM_COL32(100, 100, 255, 220),
        IM_COL32(255, 255, 100, 220),
        IM_COL32(255, 100, 255, 220),
    };
    const ImU32 lightBoxFills[] = {
        IM_COL32(255, 80,  80,  18),
        IM_COL32(80,  255, 80,  18),
        IM_COL32(80,  80,  255, 18),
        IM_COL32(255, 255, 80,  18),
        IM_COL32(255, 80,  255, 18),
    };
    const ImU32 lightBoxOutlines[] = {
        IM_COL32(255, 100, 100, 120),
        IM_COL32(100, 255, 100, 120),
        IM_COL32(100, 100, 255, 120),
        IM_COL32(255, 255, 100, 120),
        IM_COL32(255, 100, 255, 120),
    };

    // Precompute per-cascade data for drawing and stats
    struct CascadeInfo {
        float nearP, farP;
        float orthoW, orthoH;       // light ortho box size in world units
        float texelsPerUnit;         // resolution utilization
    };
    std::vector<CascadeInfo> cascadeInfos(numCascades);

    // --- Draw cascades (back to front so closer ones draw on top) ---
    for (int c = numCascades - 1; c >= 0; --c) {
        float nearP = (c == 0) ? 0.1f : shadowCascadeLevels[c - 1];
        float farP  = (c < static_cast<int>(shadowCascadeLevels.size())) ? shadowCascadeLevels[c] : cameraFarPlane;
        int colorIdx = c % 5;

        const auto proj = glm::perspective(
            glm::radians(80.0f),
            static_cast<float>(width) / static_cast<float>(height),
            nearP, farP
        );
        auto corners = getFrustumCornersWorldSpace(proj, drawView);

        // --- Camera frustum (trapezoid) ---
        if (showCameraFrustums) {
            std::vector<ImVec2> pts;
            pts.reserve(8);
            for (auto& corner : corners) {
                pts.push_back(worldToRadar(corner.x, corner.z));
            }
            ImVec2 centroid(0, 0);
            for (auto& p : pts) { centroid.x += p.x; centroid.y += p.y; }
            centroid.x /= static_cast<float>(pts.size());
            centroid.y /= static_cast<float>(pts.size());
            std::sort(pts.begin(), pts.end(), [&](const ImVec2& a, const ImVec2& b) {
                return atan2f(a.y - centroid.y, a.x - centroid.x) <
                       atan2f(b.y - centroid.y, b.x - centroid.x);
            });
            drawList->AddConvexPolyFilled(pts.data(), static_cast<int>(pts.size()), cascadeFills[colorIdx]);
            drawList->AddPolyline(pts.data(), static_cast<int>(pts.size()), cascadeOutlines[colorIdx], ImDrawFlags_Closed, 1.5f);

            // Label at centroid
            char label[8];
            snprintf(label, sizeof(label), "C%d", c);
            drawList->AddText(ImVec2(centroid.x - 6, centroid.y - 6), cascadeOutlines[colorIdx], label);
        }

        // --- Light ortho box ---
        // Reconstruct the ortho box from the light-space matrix by inverting it
        // The 8 corners of the NDC cube [-1,1]^3, transformed by inverse(lightSpaceMatrix), give the world-space ortho box
        if (showLightFrustums && c < static_cast<int>(csmLightSpaceMatrices.size())) {
            auto lightCorners = getFrustumCornersWorldSpace(
                glm::mat4(1.0f), // identity view — the lightSpaceMatrix already includes both proj and view
                csmLightSpaceMatrices[c]
            );
            // Note: getFrustumCornersWorldSpace computes inv(proj * view), so passing (identity, lsm)
            // gives inv(lsm) applied to NDC corners = world-space light box corners

            // Compute ortho dimensions from light-space bounds for stats
            glm::vec3 lCenter(0);
            for (auto& v : corners) lCenter += glm::vec3(v);
            lCenter /= static_cast<float>(corners.size());

            const glm::vec3 lightDir = getDirectionalLightDirection();
            const auto lightView = glm::lookAt(lCenter + lightDir, lCenter, glm::vec3(0, 1, 0));

            float minX = std::numeric_limits<float>::max(), maxX = std::numeric_limits<float>::lowest();
            float minY = std::numeric_limits<float>::max(), maxY = std::numeric_limits<float>::lowest();
            for (auto& v : corners) {
                auto trf = lightView * v;
                minX = std::min(minX, trf.x); maxX = std::max(maxX, trf.x);
                minY = std::min(minY, trf.y); maxY = std::max(maxY, trf.y);
            }
            cascadeInfos[c].nearP = nearP;
            cascadeInfos[c].farP  = farP;
            cascadeInfos[c].orthoW = maxX - minX;
            cascadeInfos[c].orthoH = maxY - minY;
            cascadeInfos[c].texelsPerUnit = static_cast<float>(depthMapResolution) / std::max(cascadeInfos[c].orthoW, cascadeInfos[c].orthoH);

            // Draw light ortho box projected to XZ (dashed outline)
            std::vector<ImVec2> lpts;
            lpts.reserve(8);
            for (auto& corner : lightCorners) {
                lpts.push_back(worldToRadar(corner.x, corner.z));
            }
            ImVec2 lcentroid(0, 0);
            for (auto& p : lpts) { lcentroid.x += p.x; lcentroid.y += p.y; }
            lcentroid.x /= static_cast<float>(lpts.size());
            lcentroid.y /= static_cast<float>(lpts.size());
            std::sort(lpts.begin(), lpts.end(), [&](const ImVec2& a, const ImVec2& b) {
                return atan2f(a.y - lcentroid.y, a.x - lcentroid.x) <
                       atan2f(b.y - lcentroid.y, b.x - lcentroid.x);
            });
            drawList->AddConvexPolyFilled(lpts.data(), static_cast<int>(lpts.size()), lightBoxFills[colorIdx]);
            // Dashed-look outline (thinner)
            drawList->AddPolyline(lpts.data(), static_cast<int>(lpts.size()), lightBoxOutlines[colorIdx], ImDrawFlags_Closed, 1.0f);
        }
    }

    // --- Player dot ---
    drawList->AddCircleFilled(center, 5.0f, IM_COL32(255, 255, 255, 255));
    drawList->AddCircle(center, 5.0f, IM_COL32(0, 0, 0, 200), 0, 1.5f);

    // --- Camera direction arrow ---
    glm::vec2 fwd(drawFront.x, drawFront.z);
    float fwdLen = glm::length(fwd);
    if (fwdLen > 0.001f) {
        fwd /= fwdLen;
        float arrowLen = side * 0.08f;
        ImVec2 tip(center.x + fwd.x * arrowLen, center.y + fwd.y * arrowLen);
        drawList->AddLine(center, tip, IM_COL32(255, 255, 255, 230), 2.5f);
        glm::vec2 perp(-fwd.y, fwd.x);
        float hs = 6.0f;
        ImVec2 left (tip.x - fwd.x * hs + perp.x * hs * 0.5f,
                     tip.y - fwd.y * hs + perp.y * hs * 0.5f);
        ImVec2 right(tip.x - fwd.x * hs - perp.x * hs * 0.5f,
                     tip.y - fwd.y * hs - perp.y * hs * 0.5f);
        drawList->AddTriangleFilled(tip, left, right, IM_COL32(255, 255, 255, 230));
    }

    // --- If frozen, show live camera position as a ghost ---
    if (frozen) {
        ImVec2 livePos = worldToRadar(cameraPos.x, cameraPos.z);
        drawList->AddCircleFilled(livePos, 3.0f, IM_COL32(255, 100, 100, 180));

        glm::vec2 liveFwd(cameraFront.x, cameraFront.z);
        float lfl = glm::length(liveFwd);
        if (lfl > 0.001f) {
            liveFwd /= lfl;
            float al = side * 0.05f;
            ImVec2 lt(livePos.x + liveFwd.x * al, livePos.y + liveFwd.y * al);
            drawList->AddLine(livePos, lt, IM_COL32(255, 100, 100, 150), 1.5f);
        }
    }

    // --- Sun direction indicator ---
    glm::vec2 lightXZ(directionalLightDir.x, directionalLightDir.z);
    float lLen = glm::length(lightXZ);
    if (lLen > 0.001f) {
        lightXZ /= lLen;
        float sunLen = side * 0.44f;
        ImVec2 sunPos(center.x + lightXZ.x * sunLen, center.y + lightXZ.y * sunLen);
        drawList->AddCircleFilled(sunPos, 7.0f, IM_COL32(255, 200, 50, 200));
        drawList->AddCircle(sunPos, 7.0f, IM_COL32(255, 230, 100, 255), 0, 1.5f);

        // Sun direction line from center
        drawList->AddLine(center, sunPos, IM_COL32(255, 200, 50, 60), 1.0f);
    }

    // --- Mouse hover: show world coordinate ---
    ImVec2 mousePos = ImGui::GetMousePos();
    if (mousePos.x >= canvasPos.x && mousePos.x < canvasPos.x + side &&
        mousePos.y >= canvasPos.y && mousePos.y < canvasPos.y + side) {
        float relX = (mousePos.x - center.x) / (side * 0.45f) * worldRadius + drawPos.x;
        float relZ = (mousePos.y - center.y) / (side * 0.45f) * worldRadius + drawPos.z;
        char coordTxt[64];
        snprintf(coordTxt, sizeof(coordTxt), "(%.0f, %.0f)", relX, relZ);
        drawList->AddText(ImVec2(mousePos.x + 12, mousePos.y - 8), IM_COL32(200, 200, 200, 200), coordTxt);
    }

    drawList->PopClipRect();

    // Border
    drawList->AddRect(canvasPos, ImVec2(canvasPos.x + side, canvasPos.y + side),
                      IM_COL32(80, 80, 80, 255));

    // Reserve canvas space
    ImGui::Dummy(ImVec2(side, side));

    // --- Per-cascade stats table ---
    ImGui::Separator();
    ImGui::Text("Cascade Stats (res: %u)", depthMapResolution);
    if (ImGui::BeginTable("csm_stats", 5, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("Cascade");
        ImGui::TableSetupColumn("Near");
        ImGui::TableSetupColumn("Far");
        ImGui::TableSetupColumn("Ortho Size");
        ImGui::TableSetupColumn("Texels/Block");
        ImGui::TableHeadersRow();

        for (int c = 0; c < numCascades; ++c) {
            int ci = c % 5;
            ImVec4 col;
            col.x = ((cascadeOutlines[ci] >>  0) & 0xFF) / 255.0f;
            col.y = ((cascadeOutlines[ci] >>  8) & 0xFF) / 255.0f;
            col.z = ((cascadeOutlines[ci] >> 16) & 0xFF) / 255.0f;
            col.w = 1.0f;

            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextColored(col, "C%d", c);
            ImGui::TableNextColumn(); ImGui::Text("%.1f", cascadeInfos[c].nearP);
            ImGui::TableNextColumn(); ImGui::Text("%.1f", cascadeInfos[c].farP);
            ImGui::TableNextColumn(); ImGui::Text("%.0fx%.0f", cascadeInfos[c].orthoW, cascadeInfos[c].orthoH);
            ImGui::TableNextColumn();
            float tpu = cascadeInfos[c].texelsPerUnit;
            if (tpu > 4.0f)
                ImGui::TextColored(ImVec4(0.3f, 1, 0.3f, 1), "%.1f", tpu);
            else if (tpu > 1.0f)
                ImGui::TextColored(ImVec4(1, 1, 0.3f, 1), "%.1f", tpu);
            else
                ImGui::TextColored(ImVec4(1, 0.3f, 0.3f, 1), "%.1f", tpu);
        }
        ImGui::EndTable();
    }

    ImGui::Text("Player: (%.0f, %.0f, %.0f)", drawPos.x, drawPos.y, drawPos.z);

    ImGui::End();
}


// Draws a small textured quad (preview of an FBO texture)
// defined near the bottom-left in NDC by default and can be repositioned via `offset`.
void Lighting::drawTexturePreviewQuad(const unsigned int textureID, bool grayscale, glm::vec2 offset) {
    if (textureID == 0) return;

    if (!debugFBOShader) {
        debugFBOShader = std::make_shared<Shader>(
            "shaders/debugRenderer.vert",
            "shaders/debugRenderer.frag"
        );
        debugFBOShader->use();
        debugFBOShader->setInt("fboAttachment", 0);
    }

    if (debugVAO == 0) {
        // Small NDC quad in bottom-left corner (0.20 x 0.20 NDC = ~10% screen)
        constexpr float verts[] = {
            //  pos.xy       uv
            -0.98f, -0.58f,  0.0f, 1.0f,
            -0.98f, -0.98f,  0.0f, 0.0f,
            -0.58f, -0.98f,  1.0f, 0.0f,

            -0.98f, -0.58f,  0.0f, 1.0f,
            -0.58f, -0.98f,  1.0f, 0.0f,
            -0.58f, -0.58f,  1.0f, 1.0f
       };
        glGenVertexArrays(1, &debugVAO);
        glGenBuffers(1, &debugVBO);
        glBindVertexArray(debugVAO);
        glBindBuffer(GL_ARRAY_BUFFER, debugVBO);
        glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
        glEnableVertexAttribArray(0); // position
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), static_cast<void *>(nullptr));
        glEnableVertexAttribArray(1); // uv
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), reinterpret_cast<void *>(2 * sizeof(float)));
        glBindVertexArray(0);
    }

    const GLboolean depthEnabled = glIsEnabled(GL_DEPTH_TEST);
    if (depthEnabled) glDisable(GL_DEPTH_TEST);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);

    debugFBOShader->use();
    debugFBOShader->setInt("isGrayscale", grayscale ? 1 : 0);
    debugFBOShader->setVec2("offset", offset);
    glBindVertexArray(debugVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    if (depthEnabled) glEnable(GL_DEPTH_TEST);
}

// CSM
std::vector<glm::vec4> Lighting::getFrustumCornersWorldSpace(const glm::mat4& proj, const glm::mat4& view)
{
    //  We know the coordinates of the corners of the NDC cube: the coordinates are in the range [-1,1] on the three axes.
    //  Because matrix multiplication is a reversible process, we can apply the inverse of the view and projection matrices
    //  on the corner points of the NDC cube to get the frustum corners in world space.
    const auto inv = glm::inverse(proj * view);
    
    std::vector<glm::vec4> frustumCorners;
    for (unsigned int x = 0; x < 2; ++x)
    {
        for (unsigned int y = 0; y < 2; ++y)
        {
            for (unsigned int z = 0; z < 2; ++z)
            {
                const glm::vec4 pt = 
                    inv * glm::vec4(
                        2.0f * x - 1.0f,
                        2.0f * y - 1.0f,
                        2.0f * z - 1.0f,
                        1.0f);
                frustumCorners.push_back(pt / pt.w);
            }
        }
    }
    
    return frustumCorners;
}

glm::mat4 Lighting::getLightSpaceMatrix(const float nearPlane, const float farPlane, const glm::mat4& view) const {
    const auto proj = glm::perspective(
        glm::radians(80.0f),
        (float) width / (float) height,
        nearPlane,
        farPlane
    );

    std::vector<glm::vec4> corners = getFrustumCornersWorldSpace(proj, view);

    glm::vec3 center = glm::vec3(0, 0, 0);
    for (const auto& v : corners)
    {
        center += glm::vec3(v);
    }
    center /= corners.size();

    const glm::vec3 lightDir = getDirectionalLightDirection();
    const auto lightView = glm::lookAt(
        center + lightDir,
        center,
        glm::vec3(0.0f, 1.0f, 0.0f)
    );


    float minX = std::numeric_limits<float>::max();
    float maxX = std::numeric_limits<float>::lowest();
    float minY = std::numeric_limits<float>::max();
    float maxY = std::numeric_limits<float>::lowest();
    float minZ = std::numeric_limits<float>::max();
    float maxZ = std::numeric_limits<float>::lowest();
    for (const auto& v : corners)
    {
        const auto trf = lightView * v;
        minX = std::min(minX, trf.x);
        maxX = std::max(maxX, trf.x);
        minY = std::min(minY, trf.y);
        maxY = std::max(maxY, trf.y);
        minZ = std::min(minZ, trf.z);
        maxZ = std::max(maxZ, trf.z);
    }

    // Extend the Z range to capture shadow casters behind the camera frustum.
    // A fixed extension is more predictable than the old multiplicative zMult=3.0
    // which wasted resolution by over-expanding the volume.
    // Pull near plane back to catch casters behind the view frustum,
    // and push far plane to catch distant casters.
    constexpr float zExtendBack = 150.0f;   // blocks behind the frustum
    constexpr float zExtendFront = 50.0f;   // blocks beyond the frustum
    minZ -= zExtendBack;
    maxZ += zExtendFront;

    // ── Texel-snapping ──
    // Without this, as the camera moves the ortho projection shifts by sub-texel
    // amounts, causing shadows to shimmer/swim.  We quantize the projection so
    // that each camera movement snaps to whole shadow-map texels.
    //
    // Algorithm (from NVIDIA CSM paper & OGLDev tutorial 49):
    //   1. Compute the world-space size of one shadow-map texel.
    //   2. Snap minX/maxX and minY/maxY to multiples of that size.
    const float worldUnitsPerTexelX = (maxX - minX) / static_cast<float>(depthMapResolution);
    const float worldUnitsPerTexelY = (maxY - minY) / static_cast<float>(depthMapResolution);

    minX = std::floor(minX / worldUnitsPerTexelX) * worldUnitsPerTexelX;
    maxX = std::floor(maxX / worldUnitsPerTexelX) * worldUnitsPerTexelX;
    minY = std::floor(minY / worldUnitsPerTexelY) * worldUnitsPerTexelY;
    maxY = std::floor(maxY / worldUnitsPerTexelY) * worldUnitsPerTexelY;

    const glm::mat4 lightProjection = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);

    return lightProjection * lightView;
}

std::vector<glm::mat4> Lighting::getLightSpaceMatrices(const glm::mat4& cameraView) const
{
    std::vector<glm::mat4> matrices;

    // Number of cascades = shadowCascadeLevels.size() + 1
    // Cascade 0: cameraNearPlane → shadowCascadeLevels[0]
    // Cascade 1: shadowCascadeLevels[0] → shadowCascadeLevels[1]
    // ...
    // Cascade N: shadowCascadeLevels[N-1] → cameraFarPlane

    for (size_t i = 0; i < shadowCascadeLevels.size() + 1; ++i)
    {
        float near = (i == 0) ? 0.1f : shadowCascadeLevels[i - 1];
        float far  = (i < shadowCascadeLevels.size()) ? shadowCascadeLevels[i] : cameraFarPlane;
        matrices.push_back(getLightSpaceMatrix(near, far, cameraView));
    }

    return matrices;
}

void Lighting::initCSMResources()
{
    csmDepthShader = std::make_shared<Shader>(
        "shaders/csmDepth.vert",
        "shaders/csmDepth.frag");

    const int numCascades = static_cast<int>(shadowCascadeLevels.size()) + 1;

    // Texture array: one layer per cascade
    glGenTextures(1, &csmDepthMaps);
    glBindTexture(GL_TEXTURE_2D_ARRAY, csmDepthMaps);
    glTexImage3D(
        GL_TEXTURE_2D_ARRAY,
        0,                          // mip level
        GL_DEPTH_COMPONENT32F,      // internal format (32-bit float depth)
        depthMapResolution,         // width per layer
        depthMapResolution,         // height per layer
        numCascades,                // number of layers
        0,                          // border
        GL_DEPTH_COMPONENT,         // format
        GL_FLOAT,                   // type
        nullptr                     // no data yet
    );

    // Use LINEAR + COMPARE so that each texture() call performs a
    // hardware 2×2 bilinear PCF tap, returning a smooth [0,1] value
    // instead of a binary depth.  This dramatically reduces shadow
    // flicker from leaf geometry on distant cascades.
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    float borderColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
    glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    // FBO — layer attachment is done per-pass in updateCSMShadowMaps()
    glGenFramebuffers(1, &csmFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, csmFBO);

    // Attach layer 0 initially so the FBO is complete
    glFramebufferTextureLayer(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        csmDepthMaps,
        0,                          // mip level
        0                           // layer
    );

    // We only write depth, no color output
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        std::cerr << "ERROR::CSM::FRAMEBUFFER_NOT_COMPLETE\n";

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Lighting::updateCSMShadowMaps(const Renderer& renderer, const glm::mat4& cameraView, unsigned int atlasTexture)
{
    // 1. Compute all light-space matrices for current camera position
    cachedShadowLightDir = -directionalLightDir;
    csmLightSpaceMatrices = getLightSpaceMatrices(cameraView);

    const int numCascades = static_cast<int>(csmLightSpaceMatrices.size());

    csmDepthShader->use();

    // Bind the atlas texture so the depth shader can alpha-test leaves
    if (atlasTexture != 0) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, atlasTexture);
        csmDepthShader->setInt("atlas", 0);
    }

    glViewport(0, 0, depthMapResolution, depthMapResolution);
    glBindFramebuffer(GL_FRAMEBUFFER, csmFBO);

    for (int i = 0; i < numCascades; ++i)
    {
        glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                                  csmDepthMaps, 0, i);
        glClear(GL_DEPTH_BUFFER_BIT);

        csmDepthShader->setMat4("lightSpaceMatrix", csmLightSpaceMatrices[i]);
        renderer.renderShadow(csmDepthShader, csmLightSpaceMatrices[i]);
    }

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // Restore viewport
    glViewport(0, 0, width, height);
}

void Lighting::uploadCSMUniforms(const Shader& shader, const glm::mat4& cameraView) const
{
    shader.use();

    shader.setMat4("view", cameraView);

    // Upload all light-space matrices
    for (size_t i = 0; i < shadowCascadeLevels.size() + 1; ++i)
    {
        shader.setMat4(
            "lightSpaceMatrices[" + std::to_string(i) + "]",
            csmLightSpaceMatrices[i]);
    }

    // Upload the cascade far-plane distances (view-space Z values).
    // The fragment shader compares the fragment's view-space depth
    // against these to pick the right cascade.
    for (size_t i = 0; i < shadowCascadeLevels.size(); ++i)
    {
        shader.setFloat(
            "cascadePlaneDistances[" + std::to_string(i) + "]",
            shadowCascadeLevels[i]);
    }

    shader.setInt("cascadeCount", static_cast<int>(shadowCascadeLevels.size()) + 1);
    shader.setFloat("farPlane", cameraFarPlane);

    // Bind the CSM depth texture array to texture unit 7.
    glActiveTexture(GL_TEXTURE7);
    glBindTexture(GL_TEXTURE_2D_ARRAY, csmDepthMaps);
    shader.setInt("shadowMapArray", 7);

    shader.setInt("debugCascades", debugCascades);
}

// Setters
void Lighting::setPointLightEnabled(const int index, const bool enabled) {
    if (index < 0 || index >= 3) return;
    pointLightsOn[index] = enabled;
}

void Lighting::setPointLightPosition(int index, const glm::vec3 &pos) {
    if (index < 0 || index >= 3) return;
    pointLightPositions[index] = pos;
}

void Lighting::setPointLightAmbient(int index, const glm::vec3& color) {
    if (index < 0 || index >= 3) return;
    pointLightAmbient[index] = color;
}

void Lighting::setPointLightDiffuse(int index, const glm::vec3 &color) {
    if (index < 0 || index >= 3) return;
    pointLightDiffuse[index] = color;
}

void Lighting::setPointLightSpecular(int index, const glm::vec3 &color) {
    if (index < 0 || index >= 3) return;
    pointLightSpecular[index] = color;
}

void Lighting::setPointLightConstant(int index, const float constant) {
    if (index < 0 || index >= 3) return;
    pointLightConstant[index] = constant;
}

void Lighting::setPointLightLinear(int index, const float linear) {
    if (index < 0 || index >= 3) return;
    pointLightLinear[index] = linear;
}

void Lighting::setPointLightQuadratic(int index, const float quadratic) {
    if (index < 0 || index >= 3) return;
    pointLightQuadratic[index] = quadratic;
}


// Getters
bool Lighting::isPointLightOn(const int index) const {
    if (index < 0 || index >= 3) return false;
    return pointLightsOn[index];
};
glm::vec3 Lighting::getPointLightPosition(const int index) const {
    if (index < 0 || index >= 3) return glm::vec3(0.0f);
    return pointLightPositions[index]; 
};
glm::vec3 Lighting::getPointLightAmbient(const int index) const {
    if (index < 0 || index >= 3) return glm::vec3(0.0f);
    return pointLightAmbient[index]; 
};
glm::vec3 Lighting::getPointLightDiffuse(const int index) const {
    if (index < 0 || index >= 3) return glm::vec3(0.0f);
    return pointLightDiffuse[index]; 
};
glm::vec3 Lighting::getPointLightSpecular(const int index) const {
    if (index < 0 || index >= 3) return glm::vec3(0.0f);
    return pointLightSpecular[index]; 
};
float Lighting::getPointLightConstant(const int index) const {
    if (index < 0 || index >= 3) return 1.0f;
    return pointLightConstant[index]; 
};
float Lighting::getPointLightLinear(const int index) const {
    if (index < 0 || index >= 3) return 0.0f;
    return pointLightLinear[index]; 
};
float Lighting::getPointLightQuadratic(const int index) const {
    if (index < 0 || index >= 3) return 0.0f;
    return pointLightQuadratic[index]; 
};
