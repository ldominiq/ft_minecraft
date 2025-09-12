//
// Created by lucas on 6/25/25.
//

#include "App.hpp"


App::App(): VAO(0),
			VBO(0),
			EBO(0),

			shaderProgram(0),
			texture(0),

			camera(nullptr),
			monitor(nullptr),
			mode(nullptr),

            skybox(nullptr),
            textureShader(nullptr),
            gradientShader(nullptr),
            activeShader(nullptr) {

    // Pre-allocate the FPS sample buffer to avoid reallocations at runtime
    fpsSamples.reserve(fpsSampleCount);
}

App::~App() { cleanup(); }

void App::init() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    // Monitor infos
    monitor = glfwGetPrimaryMonitor();
    mode = glfwGetVideoMode(monitor);

    window = glfwCreateWindow(windowedWidth, windowedHeight, "ft_minecraft", nullptr, nullptr);
    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, const int width, const int height) {
        (void)w;
        glViewport(0, 0, width, height);
    });

    glfwMakeContextCurrent(window);
    gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));

    // VAO for fullscreen triangle (no attributes needed)
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);
    glBindVertexArray(0);

    glfwGetFramebufferSize(window, &windowedWidth, &windowedHeight);

    const std::vector<std::string> faces = {
        "assets/skybox/right.bmp",  // +X
        "assets/skybox/left.bmp",   // -X
        "assets/skybox/top.bmp",    // +Y
        "assets/skybox/bottom.bmp", // -Y
        "assets/skybox/front.bmp",  // +Z
        "assets/skybox/back.bmp"    // -Z
    };

	skybox = std::make_unique<Skybox>(faces);

	udpClient = std::make_unique<UDPClient>("127.0.0.1");
	setUdpClientPacketCallback();

	renderer = std::make_unique<Renderer>();

    // Setup light cube geometry (a unit cube) for visualization of point lights
    glGenVertexArrays(1, &lightCubeVAO);
    glGenBuffers(1, &lightCubeVBO);
    glBindVertexArray(lightCubeVAO);
    glBindBuffer(GL_ARRAY_BUFFER, lightCubeVBO);
    static const float lightCubeVertices[] = {
        // positions only (36 vertices -> 12 triangles)
        -0.5f, -0.5f, -0.5f, // Front face
         0.5f, -0.5f, -0.5f, 
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f, -0.5f,
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,

        -0.5f, -0.5f,  0.5f, // Back face
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,

        -0.5f,  0.5f,  0.5f, // Left face
        -0.5f,  0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f, -0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,

         0.5f,  0.5f,  0.5f, // Right face
         0.5f,  0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,

        -0.5f, -0.5f, -0.5f, // Bottom face
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f,

        -0.5f,  0.5f, -0.5f, // Top face
         0.5f,  0.5f, -0.5f,
         0.5f,  0.5f,  0.5f,
         0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f,  0.5f,
        -0.5f,  0.5f, -0.5f,

        -0.5f, -0.5f, -0.5f, 
         0.5f, -0.5f, -0.5f,
         0.5f, -0.5f,  0.5f,
         0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f,  0.5f,
        -0.5f, -0.5f, -0.5f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(lightCubeVertices), lightCubeVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);

    
    glEnable(GL_DEPTH_TEST);
    
    // enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // virus
    //glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    // SHADOW MAPPING
    // ===============================================================

    // Create a 2D texture that we'll use as the framebuffer's depth buffer
    glGenTextures(1, &depthMap);
    glBindTexture(GL_TEXTURE_2D, depthMap);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, 
             SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT); 
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // With the generated depth texture we can attach it as the framebuffer's depth buffer
    glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMap, 0);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0); 

    // Mouse movement event handling
    camera = std::make_unique<Camera>(glm::vec3(0.0f, 128.0f, 0.0f));
    glfwSetWindowUserPointer(window, this);
    glfwSetCursorPosCallback(window, [](GLFWwindow* w, const double xpos, const double ypos) {
        static App* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        if (!app) return;
        // Honour ImGui’s mouse capture: if the UI is being interacted with
        // (e.g. hovering/clicking in a window), do not rotate the camera.
        ImGuiIO& io = ImGui::GetIO();

        if (io.WantCaptureMouse || app->uiInteractive) {
            return;
        }
        if (app->firstMouse) {
            app->lastX = xpos;
            app->lastY = ypos;
            app->firstMouse = false;
        }
        const float xoffset = static_cast<float>(xpos - app->lastX);
        const float yoffset = static_cast<float>(app->lastY - ypos); // Reversed: y-coordinates go from bottom to top
        app->lastX = xpos;
        app->lastY = ypos;
        app->camera->processMouseMovement(xoffset, yoffset);

		app->keyPressedRecently = true;
    });
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);


	glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
		App* app = static_cast<App*>(glfwGetWindowUserPointer(w));
		if (!app) return;

		auto mapKeyToBit = [](int key) -> uint16_t {
			switch (key) {
				case GLFW_KEY_W: return IN_FORWARD;
				case GLFW_KEY_S: return IN_BACKWARD;
				case GLFW_KEY_A: return IN_LEFT;
				case GLFW_KEY_D: return IN_RIGHT;
				case GLFW_KEY_SPACE: return IN_UP;   // jump
				case GLFW_KEY_LEFT_SHIFT: return IN_RUN;
				case GLFW_KEY_LEFT_CONTROL: return IN_DOWN;
				default: return 0; // key not tracked
			}
		};

		uint16_t bit = mapKeyToBit(key);
		if (!bit) return; // not an input we care about

		if (action == GLFW_PRESS || action == GLFW_REPEAT) {
			app->inputMask |= bit;              // set bit
			app->keyPressedRecently = true;
		} 
		else if (action == GLFW_RELEASE) {
			app->inputMask &= ~bit;             // clear bit
			app->keyPressedRecently = (app->inputMask != 0);
		}
	});


	glfwSetMouseButtonCallback(window, [](GLFWwindow* w, int button, int action, int mods) {
		App* app = static_cast<App*>(glfwGetWindowUserPointer(w));
		if (!app) return;

		//kinda weird way to do it.
		uint8_t mouseButtons = 0;
		if (action == GLFW_PRESS) {
			if (button == GLFW_MOUSE_BUTTON_LEFT) {
				mouseButtons |= IN_LEFT_CLICK;
			} else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
				mouseButtons |= IN_RIGHT_CLICK;
			}
		}

		NetPlayerMouseInputs pkt;
		pkt.mouseButtons = mouseButtons;
		app->udpClient->sendPacket(pkt);

		// else if (action == GLFW_RELEASE) {
		// 	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		// 		app->onLeftClickReleased();
		// 	} else if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		// 		app->onRightClickReleased();
		// 	}
		// }
	});

    // -------------------------------------------------------------------------
    // ImGui initialization
    // Create ImGui context and set up GLFW/OpenGL bindings.  We specify the
    // OpenGL version used by the backend (matching the context created above).
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // Set a dark theme
    ImGui::StyleColorsDark();
    // Initialize ImGui for GLFW and OpenGL.  Pass the window pointer and GLSL
    // version string.  The GLSL version must match your context version.
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 460");

	loadControlsFromFile();
}

void App::setUdpClientPacketCallback()
{
	udpClient->setCallback([this](const PacketPtr& pkt) {

		switch (pkt->type) {
			case PacketType::NET_ACCEPT: {
				auto& p = static_cast<NetAccept&>(*pkt);
				std::cout << "Client accepted! id=" << p.clientId << "\n";
				clientConnected = true;
				break;
			}

			case PacketType::CHUNK_HEADER: {
				auto& p = static_cast<NetChunkHeader&>(*pkt);
				// handle chunk data (append to buffer, etc.)
				renderer->prepareChunk(p);
				break;
			}

			case PacketType::CHUNK_DATA: {
				auto& p = static_cast<NetChunkData&>(*pkt);
				// handle chunk data (append to buffer, etc.)
				renderer->receiveChunk(p);
				break;
			}

			case PacketType::PLAYER_MOVE: {
				auto& p = static_cast<NetPlayerMove&>(*pkt);
				camera->updatePosition(p);
				break;
			}

			case PacketType::MODIFIED_BLOCK_DATA: {
				auto& p = static_cast<NetModifiedBlockData&>(*pkt);
				renderer->updateChunk(p);
				break;
			}

            case PacketType::NET_IMGUI: {
                auto& p = static_cast<NetImGui&>(*pkt);
                // handle ImGui data (e.g., update UI state)
                currentBiome = p.currentBiome;
                break;
            }

			// case PacketType::UPDATE_WORLD: {
			//     auto& p = static_cast<UpdateWorld&>(*pkt);
			//     // handle movement/world updates
			//     applyWorldUpdate(p);
			//     break;
			// }

			default:
				std::cout << "Unknown packet type: " << static_cast<int>(pkt->type) << "\n";
				break;
		}
	});
}


void App::loadResources() {
    // Load shaders and textures

    textureShader = std::make_shared<Shader>("shaders/simple.vert", "shaders/simple.frag");
    gradientShader = std::make_shared<Shader>("shaders/gradient.vert", "shaders/gradient.frag");
    skyShader = std::make_shared<Shader>("shaders/sky.vert", "shaders/sky.frag");
    lightCubeShader = std::make_shared<Shader>("shaders/lightCubeShader.vert", "shaders/lightCubeShader.frag");
    simpleDepthShader = std::make_shared<Shader>("shaders/simpleDepthShader.vert", "shaders/simpleDepthShader.frag");
    debugDepthQuad = std::make_shared<Shader>("shaders/debugDepthQuad.vert", "shaders/debugDepthQuad.frag");
    texture = loadTexture("assets/textures/textures.png");

    activeShader = textureShader;

    activeShader->use();
    activeShader->setInt("atlas", 0);
}

void App::render() {
    

    while (!glfwWindowShouldClose(window)) {

		//sending/receiving packets and stuff
		udpClient->receivePacket();
		if (keyPressedRecently)
		{
			NetPlayerInputs inputs = buildPlayerInputsPacket();
			udpClient->sendPacket(inputs);
		}


        // Calculate delta time for frame rate
        const float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // Maintain a moving average of the last N frame times for a stable
        // FPS display.  Push the current frame time and pop the oldest if
        // the buffer is full.  Then compute the average delta and convert
        // to FPS.  If the buffer is empty (first frame) we simply use the
        // current frame rate.
        fpsSamples.push_back(deltaTime);
        if (fpsSamples.size() > fpsSampleCount) {
            fpsSamples.erase(fpsSamples.begin());
        }
        float avgDelta = 0.0f;
        for (float dt : fpsSamples) {
            avgDelta += dt;
        }
        if (!fpsSamples.empty()) {
            avgDelta /= static_cast<float>(fpsSamples.size());
            uiDisplayFPS = avgDelta > 0.0f ? 1.0f / avgDelta : 0.0f;
        } else {
            uiDisplayFPS = 0.0f;
        }

        // Start a new ImGui frame.  We do this **before** handling input so that
        // ImGui’s internal state (WantCaptureMouse/WantCaptureKeyboard) is up
        // to date for this frame.  Even if the UI is currently hidden we
        // create a new frame to ensure ImGui updates its internal state and
        // resets input capture flags.
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        updateWindowTitle();
		
        processInput();

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        

        // --- Draw sky background first ---
        skyShader->use();
        // window aspect / uniforms
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        const float aspect = static_cast<float>(width) / static_cast<float>(height);

        glm::mat4 view = camera->getViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(80.0f), aspect, 0.1f, renderDistance);


        // Shadow mapping
        // ====================================
        // 1. render depth of scene to texture (from light's perspective)
        // --------------------------------------------------------------
        glm::vec3 lightPos(0.0f, 60.0f, 0.0f); // bigger area
        float orthoSize = 120.0f;
        float near_plane = 1.0f, far_plane = 300.0f;
        glm::mat4 lightProjection = glm::ortho(-orthoSize, orthoSize,
                                               -orthoSize, orthoSize,
                                               near_plane, far_plane);
        glm::mat4 lightView = glm::lookAt(lightPos,
                                          lightPos + directionalLightDir,
                                          glm::vec3(0,1,0));
        glm::mat4 lightSpaceMatrix = lightProjection * lightView;
        // 1. Depth pass
        glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
        glBindFramebuffer(GL_FRAMEBUFFER, depthMapFBO);
        glClear(GL_DEPTH_BUFFER_BIT);
        glCullFace(GL_FRONT); // reduce peter-panning
        simpleDepthShader->use();
        simpleDepthShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
        simpleDepthShader->setMat4("model", glm::mat4(1.0f));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        renderer->render(simpleDepthShader); // ensure it sets model if needed
        glCullFace(GL_BACK);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);

        // Restore viewport
        glViewport(0, 0, width, height);

        // 2. Sky (as you already do) then 3. World pass:
        activeShader->use();
        activeShader->setMat4("lightSpaceMatrix", lightSpaceMatrix);
        activeShader->setMat4("model", glm::mat4(1.0f));
        activeShader->setInt("shadowMap", 1);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, depthMap);

        renderer->render(activeShader);
        

        if (quadVAO == 0)
        {
            float quadVertices[] = {
                // positions        // texture Coords
                -1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
                -1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
                1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
                1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
            };
            // setup plane VAO
            glGenVertexArrays(1, &quadVAO);
            glGenBuffers(1, &quadVBO);
            glBindVertexArray(quadVAO);
            glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
            glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
            glEnableVertexAttribArray(1);
            glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
        }
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        // If you want to visualize something (e.g., depth) bind the proper shader/texture here.
        debugDepthQuad->use();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, depthMap);
        debugDepthQuad->setInt("depthMap", 0);
        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glBindVertexArray(0);

        // ====================================

        // Time management for sky shader
        if (skyTimePaused == false)
            skyTimeOffset += deltaTime * 0.05f; // Speed of sun movement

        const float timeScale = 0.2f;
        const float t = skyTimeOffset * timeScale;
        glm::vec3 sunDir = glm::normalize(glm::vec3(
            std::sin(t), // x (azimuth)
            std::cos(t), // y (elevation)
            0.0f));     // z

        skyShader->setVec2("resolution", glm::vec2(width, height));
        skyShader->setFloat("time", skyTimeOffset);
        skyShader->setMat4("view", view);
        skyShader->setMat4("projection", projection);
        skyShader->setVec3("cameraPosWorld", camera->Position);
        skyShader->setFloat("seaLevel", 64.0f);
        skyShader->setFloat("exposure", skyExposure);
        skyShader->setFloat("atmDensity", skyAtmDensity);
        skyShader->setFloat("atmThickness", skyAtmThickness);
        skyShader->setFloat("planetScale", planetScale);
        skyShader->setVec3("sunDir", sunDir);

        // Disable depth test and writes for background
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);
        glBindVertexArray(VAO);
        glDrawArrays(GL_TRIANGLES, 0, 3);
        glBindVertexArray(0);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        activeShader->use();

        // Lighting uniforms
        // ====================================
        

        activeShader->setVec3("viewPos", camera->Position);

        activeShader->setFloat("material.shininess", materialShininess);

        // directional light
        if (directionalLightOn) {
            
            directionalLightDir = -sunDir; // Direction from which the light is coming (sun direction)

            // day/night factror based on sun elevation
            float day = glm::clamp(sunDir.y * 0.7f, 0.0f, 1.0f);
            // smooth transition near sunset/sunrise
            day = glm::smoothstep(0.0f, 1.0f, day);

            // small ambiant light at night
            const float nightAmbientMin = 0.3f;
            glm::vec3 ambientColor = directionalAmbientColor * (nightAmbientMin + (1.0f - nightAmbientMin) * day);
            glm::vec3 diffuseColor = directionalDiffuseColor * day;
            glm::vec3 specularColor = directionalSpecularColor * day;
            activeShader->setVec3("dirLight.direction", directionalLightDir);
            activeShader->setVec3("dirLight.ambient", ambientColor);
            activeShader->setVec3("dirLight.diffuse", diffuseColor);
            activeShader->setVec3("dirLight.specular", specularColor);
        } else {
            activeShader->setVec3("dirLight.ambient", glm::vec3(0.0f));
            activeShader->setVec3("dirLight.diffuse", glm::vec3(0.0f));
            activeShader->setVec3("dirLight.specular", glm::vec3(0.0f));
        }
        // point lights
        for ( int i=0; i < 4; i++ ) {
            if (!pointLightsOn[i]) {
                activeShader->setVec3("pointLights[" + std::to_string(i) + "].ambient", glm::vec3(0.0f));
                activeShader->setVec3("pointLights[" + std::to_string(i) + "].diffuse", glm::vec3(0.0f));
                activeShader->setVec3("pointLights[" + std::to_string(i) + "].specular", glm::vec3(0.0f));
                continue;
            }
            activeShader->setVec3("pointLights[" + std::to_string(i) + "].position", pointLightPositions[i]);
            activeShader->setVec3("pointLights[" + std::to_string(i) + "].ambient", pointLightAmbient[i]);
            activeShader->setVec3("pointLights[" + std::to_string(i) + "].diffuse", pointLightDiffuse[i]);
            activeShader->setVec3("pointLights[" + std::to_string(i) + "].specular", pointLightSpecular[i]);
            activeShader->setFloat("pointLights[" + std::to_string(i) + "].constant", pointLightConstant[i]);
            activeShader->setFloat("pointLights[" + std::to_string(i) + "].linear", pointLightLinear[i]);
            activeShader->setFloat("pointLights[" + std::to_string(i) + "].quadratic", pointLightQuadratic[i]);
        }
        // spotLight (flashlight)
        if (flashlightOn) {
            activeShader->setVec3("spotLight.position", camera->Position);
            activeShader->setVec3("spotLight.direction", camera->Front);
            activeShader->setVec3("spotLight.ambient", glm::vec3(0.0f));
            activeShader->setVec3("spotLight.diffuse", glm::vec3(1.0f));
            activeShader->setVec3("spotLight.specular", glm::vec3(1.0f));
            activeShader->setFloat("spotLight.constant", spotLightConstant);
            activeShader->setFloat("spotLight.linear", spotLightLinear);
            activeShader->setFloat("spotLight.quadratic", spotLightQuadratic);
            activeShader->setFloat("spotLight.cutOff", glm::cos(glm::radians(flashlightCutoff)));
            activeShader->setFloat("spotLight.outerCutOff", glm::cos(glm::radians(flashlightOuterCutoff)));
        } else {
            activeShader->setVec3("spotLight.ambient", glm::vec3(0.0f));
            activeShader->setVec3("spotLight.diffuse", glm::vec3(0.0f));
            activeShader->setVec3("spotLight.specular", glm::vec3(0.0f));
        }


        // Set the uniform matrices in the shader
        activeShader->setMat4("view", view);
        activeShader->setMat4("projection", projection);

        // also draw the lamp object(s)
        lightCubeShader->use();
        lightCubeShader->setMat4("projection", projection);
        lightCubeShader->setMat4("view", view);

        glm::mat4 model = glm::mat4(1.0f);
        // also draw the lamp object(s)
        lightCubeShader->use();
        lightCubeShader->setMat4("projection", projection);
        lightCubeShader->setMat4("view", view);

        // we now draw as many light bulbs as we have point lights.
        glBindVertexArray(lightCubeVAO);
        for (unsigned int i = 0; i < 4; i++)
        {
            model = glm::mat4(1.0f);
            model = glm::translate(model, pointLightPositions[i]);
            model = glm::scale(model, glm::vec3(0.2f)); // Make it a smaller cube
            // Set per-cube color here so each light uses its own color
            glm::vec3 cubeCol = pointLightsOn[i] ? pointLightDiffuse[i] : glm::vec3(0.0f);
            lightCubeShader->setVec3("cubeColor", cubeCol);
            lightCubeShader->setMat4("model", model);
            glDrawArrays(GL_TRIANGLES, 0, 36);
        }


		const int currentChunkX = static_cast<int>(std::floor(camera->Position.x / Chunk::WIDTH));
		const int currentChunkZ = static_cast<int>(std::floor(camera->Position.z / Chunk::DEPTH));

		renderer->buildChunks();
		renderer->organizeChunks(Chunk::toKey(currentChunkX, currentChunkZ));

        renderer->render(activeShader);


        // skybox->draw(camera->getViewMatrix(), projection);
        camera->drawWireframeSelectedBlockFace(renderer, view, projection);
        
        glBindVertexArray(0);

        if (showDebugWindow) {
            //ImGui::ShowDemoWindow();
            debugWindow();
        }

        // Finalize the ImGui frame and draw it.  Even if the overlay is
        // non-interactive the draw data will be present, so draw it always.
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Swap buffers and poll events (keys pressed, mouse movement, etc.)
        glfwSwapBuffers(window);
        glfwPollEvents();
    }
}

void App::debugWindow() {
        // Build the ImGui UI.  We always draw the debug overlay.  When
        // uiInteractive is false we disable input on the window, allowing
        // the player to interact with the game while the overlay remains
        // visible.  When uiInteractive is true the window captures input and
        // the mouse is released.
        {
            ImGuiStyle& style = ImGui::GetStyle();

            // Set default font size
            static bool appliedDefaultFontSize = false;
            if (!appliedDefaultFontSize) {
                style.FontSizeBase = 30.0f;
                style._NextFrameFontSizeBase = 30.0f; // FIXME: Temporary hack until we finish remaining work.
                appliedDefaultFontSize = true;
            }

            glm::vec3 pos = camera->Position;
            int wx = static_cast<int>(std::floor(pos.x));
            int wz = static_cast<int>(std::floor(pos.z));
            int wy = static_cast<int>(std::floor(pos.y));
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
            if (!uiInteractive) {
                flags |= ImGuiWindowFlags_NoInputs;
                // Make the overlay slightly transparent when not interactive
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
            }
            ImGui::Begin("Debug Window", nullptr, flags);
            
            ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
            if (ImGui::BeginTabBar("Tabs", tab_bar_flags))
            {
                if (ImGui::BeginTabItem("ALL"))
                {
                    // Display smoothed FPS and frame time
                    ImGui::Text("FPS: %.1f (%.3f ms)", uiDisplayFPS, uiDisplayFPS > 0.0f ? 1000.0f / uiDisplayFPS : 0.0f);
                    // Display camera coordinates
                    ImGui::Text("Camera Position: x=%d y=%d z=%d", wx, wy, wz);

                    // ImGui::Text("World SEED: %i", params.seed);

                    // ImGui::Text("Continentalness: %.3f", Chunk::getContinentalness(params, wx, wz));
                    // ImGui::Text("Erosion: %.3f", Chunk::getErosion(params, wx, wz));
                    // ImGui::Text("Peak/Valley: %.3f", Chunk::getPV(params, wx, wz));
                    // ImGui::Text("Temperature: %.3f", Chunk::getTemperature(params, wx, wz));
                    // ImGui::Text("Humidity: %.3f", Chunk::getHumidity(params, wx, wz));

                    uint8_t biome = currentBiome;
                    const char* biomeName =
                        (static_cast<BiomeType>(biome) == BiomeType::PLAINS) ? "PLAINS" :
                        (static_cast<BiomeType>(biome) == BiomeType::DESERT) ? "DESERT" :
                        (static_cast<BiomeType>(biome) == BiomeType::FOREST) ? "FOREST" :
                        (static_cast<BiomeType>(biome) == BiomeType::TUNDRA) ? "TUNDRA" :
                        (static_cast<BiomeType>(biome) == BiomeType::SWAMP)  ? "SWAMP"  :
                        (static_cast<BiomeType>(biome) == BiomeType::OCEAN)  ? "OCEAN"  :
                        (static_cast<BiomeType>(biome) == BiomeType::MOUNTAIN) ? "MOUNTAIN" :
                                                    "UNKNOWN";
                    ImGui::Text("BIOME: %s", biomeName);


                    // Additional metrics: number of loaded chunks and approximate memory usage
                    if (renderer) {
                        const size_t visibleChunks = renderer->getVisibleChunkCount();
                        const size_t totalChunks   = renderer->getTotalChunkInMemoryCount();
                        ImGui::Text("Chunks: %zu visible / %zu total", visibleChunks, totalChunks);
                    }
                    // Display memory usage in megabytes.  We call a static helper to
                    // obtain the current resident set size (RSS).
                    {
                        const size_t memBytes = getCurrentRSS();
                        const double memMB = memBytes / (1024.0 * 1024.0);
                        ImGui::Text("Memory: %.2f MB", memMB);
                    }

                    ImGui::Separator();

                    if (ImGui::CollapsingHeader("Teleportation")) {
                        // Teleport player
                        ImGui::Text("Teleport Player");
                        static float tmpX = 0;
                        static float tmpY = 100;
                        static float tmpZ = 0;
                        ImGui::InputFloat("X", &tmpX);
                        ImGui::InputFloat("Y", &tmpY);
                        ImGui::InputFloat("Z", &tmpZ);
                        if (ImGui::Button("Teleport")) {
                            camera->Position = glm::vec3(tmpX, tmpY, tmpZ);
                        }
                    }

                    ImGui::Separator();

                    // if (ImGui::CollapsingHeader("Noise Generation")) {
                    //     if (ImGui::CollapsingHeader("Continentalness Parameters")) {
                    //         ImGui::SliderFloat("frequency", &params.continentalnessFrequency, 0.001f, 0.01f);
                    //         ImGui::SliderInt("octaves", &params.continentalnessOctaves, 1, 10);
                    //         ImGui::SliderFloat("persistence", &params.continentalnessPersistence, 0.0f, 1.0f);
                    //         ImGui::SliderFloat("lacunarity", &params.continentalnessLacunarity, 1.0f, 4.0f);
                    //         ImGui::SliderFloat("scaling factor", &params.continentalnessScalingFactor, 1.0f, 5.0f);
                    //     }

                    //     if (ImGui::CollapsingHeader("Erosion Parameters")) {
                    //         ImGui::SliderFloat("#frequency", &params.erosionFrequency, 0.001f, 0.02f);
                    //         ImGui::SliderInt("#octaves", &params.erosionOctaves, 1, 10);
                    //         ImGui::SliderFloat("#persistence", &params.erosionPersistence, 0.0f, 1.0f);
                    //         ImGui::SliderFloat("#lacunarity", &params.erosionLacunarity, 1.0f, 4.0f);
                    //         ImGui::SliderFloat("#scaling factor", &params.erosionScalingFactor, 1.0f, 5.0f);
                    //     }

                    //     if (ImGui::CollapsingHeader("Peak/Valley Parameters")) {
                    //         ImGui::SliderFloat("-frequency", &params.peakValleyFrequency, 0.001f, 0.09f);
                    //         ImGui::SliderInt("-octaves", &params.peakValleyOctaves, 1, 10);
                    //         ImGui::SliderFloat("-persistence", &params.peakValleyPersistence, 0.0f, 1.0f);
                    //         ImGui::SliderFloat("-lacunarity", &params.peakValleyLacunarity, 1.0f, 4.0f);
                    //         ImGui::SliderFloat("-scaling factor", &params.peakValleyScalingFactor, 1.0f, 5.0f);
                    //     }

                    //     if (ImGui::CollapsingHeader("Temperature Parameters")) {
                    //         ImGui::SliderFloat("--frequency", &params.temperatureFrequency, 0.0001f, 0.0012f);
                    //         ImGui::SliderInt("--octaves", &params.temperatureOctaves, 1, 10);
                    //         ImGui::SliderFloat("--persistence", &params.temperaturePersistence, 0.0f, 1.0f);
                    //         ImGui::SliderFloat("--lacunarity", &params.temperatureLacunarity, 1.0f, 4.0f);
                    //         ImGui::SliderFloat("--scaling factor", &params.temperatureScalingFactor, 1.0f, 5.0f);
                    //     }

                    //     if (ImGui::CollapsingHeader("Humidity Parameters")) {
                    //         ImGui::SliderFloat("---frequency", &params.humidityFrequency, 0.0005f, 0.0015f);
                    //         ImGui::SliderInt("---octaves", &params.humidityOctaves, 1, 10);
                    //         ImGui::SliderFloat("---persistence", &params.humidityPersistence, 0.0f, 1.0f);
                    //         ImGui::SliderFloat("---lacunarity", &params.humidityLacunarity, 1.0f, 4.0f);
                    //         ImGui::SliderFloat("---scaling factor", &params.humidityScalingFactor, 1.0f, 5.0f);
                    //     }
                    // }
                    

                    ImGui::Separator();

                    // if (ImGui::CollapsingHeader("Heightmap")) {
                    //     // Create heightmap image
                    //     ImGui::Text("Heightmap Generation");
                    //     ImGui::InputInt("Size (ex. 100)", &params.genSize);
                    //     ImGui::InputInt("Downsample (ex. 8)", &params.downsample);
                    //     if (ImGui::Button("Generate Noises")) {
                    //         if (world) {
                    //             world->dumpHeightmap(0, 0, params.genSize, params.genSize, params.downsample, 1);
                    //         }
                    //     }
                    //     if (ImGui::Button("Generate Heightmaps")) {
                    //         if (world) {
                    //             world->dumpHeightmap(0, 0, params.genSize, params.genSize, params.downsample, 0);
                    //         }
                    //     }
                    //     if (ImGui::Button("Generate Biome Map")) {
                    //         if (world) {
                    //             world->dumpBiomeMap(0, 0, params.genSize, params.genSize, params.downsample);
                    //         }
                    //     }
                    // }

                    ImGui::Separator();

                    if (ImGui::CollapsingHeader("Rendering")) {
                        ImGui::Text("Rendering Options");
                        // Wireframe toggle
                        if (ImGui::Checkbox("Wireframe", &wireframe)) {
                            glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
                        }
                        // Shader toggle (texture vs gradient).  We update activeShader accordingly.
                        if (ImGui::Checkbox("Use Gradient Shader", &useGradientShader)) {
                            activeShader = useGradientShader ? gradientShader : textureShader;
                        }
                        static int renderType = 0;
                        ImGui::RadioButton("Lighting render", &renderType, 0); ImGui::SameLine();
                        ImGui::RadioButton("Normals render",  &renderType, 1); ImGui::SameLine();
                        ImGui::RadioButton("Depth render",    &renderType, 2);

                        // Ensure the uniform is applied to the intended program(s),
                        // not whatever was last bound (e.g., selected-face wireframe).
                        auto applyRenderType = [&](const std::shared_ptr<Shader>& s) {
                            if (!s) return;
                            s->use();
                            s->setInt("renderType", renderType);
                        };
                        applyRenderType(textureShader);

                        static bool useBlinnPhong = true;
                        if (ImGui::Checkbox("Blinn-Phong", &useBlinnPhong)) {
                            textureShader->use();
                            textureShader->setInt("blinn", useBlinnPhong);
                        }
                        // Changing this will update the far clipping plane.
                        // ImGui::SliderFloat("Clipping plane Distance", &renderDistance, 100.0f, 2000.0f);
                        
                        // Adjust the chunk loading radius.  Casting to int and back avoids
                        // accidental type issues in the setter.  We clamp the range to a
                        // reasonable minimum and maximum.
                        if (renderer) {
                            int radius = static_cast<int>(renderer->getLoadRadius());
                            if (ImGui::SliderInt("Chunk Load Radius", &radius, 4, 32)) {
                                renderer->setLoadRadius(radius);
                            }
                        }
    
                        // Adjust the maximum number of chunks being generated at the same time.
                        // Lower values produce smoother frame rates but slower world loading.
                        // if (world) {
                        //     int maxGen = static_cast<int>(world->getMaxConcurrentGeneration());
                        //     if (ImGui::SliderInt("Generation Concurrency", &maxGen, 1, 8)) {
                        //         world->setMaxConcurrentGeneration(static_cast<std::size_t>(maxGen));
                        //     }
                        // }
                    }

                    // Lighting controls: direction and colours.  The direction vector
                    // components are clamped to [-1,1]; colours use a colour picker.
                    ImGui::Separator();
                    if (ImGui::CollapsingHeader("Lighting")) {
                        if (ImGui::BeginTabBar("Lighting", tab_bar_flags))
                        {
                            if (ImGui::BeginTabItem("Directional Light"))
                            {
                                ImGui::Text("Directional Light Controls");
                                ImGui::Checkbox("Light On", &directionalLightOn);
                                ImGui::SliderFloat3("Light Direction", &directionalLightDir.x, -1.0f, 1.0f);
                                ImGui::ColorEdit3("Light Colour", &directionalDiffuseColor.x);
                                ImGui::ColorEdit3("Ambient Colour", &directionalAmbientColor.x);
                                ImGui::ColorEdit3("Specular Colour", &directionalSpecularColor.x);
                                ImGui::SliderFloat("Material Shininess", &materialShininess, 1.0f, 256.0f);
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Point Lights"))
                            {
                                ImGui::Text("Point Light Controls");
                                for (int i = 0; i < pointLightsOn.size(); ++i)
                                {
                                    bool enabled = pointLightsOn[i];
                                    if (ImGui::Checkbox(("Light " + std::to_string(i)).c_str(), &enabled)) {
                                        pointLightsOn[i] = enabled;
                                    }
                                    ImGui::SliderFloat3(("Light " + std::to_string(i) + " Position").c_str(), &pointLightPositions[i].x, 0.0f, 90.0f);
                                    ImGui::SliderFloat(("Light " + std::to_string(i) + " Constant").c_str(), &pointLightConstant[i], 0.0f, 2.0f);
                                    ImGui::SliderFloat(("Light " + std::to_string(i) + " Linear").c_str(), &pointLightLinear[i], 0.0f, 0.2f);
                                    ImGui::SliderFloat(("Light " + std::to_string(i) + " Quadratic").c_str(), &pointLightQuadratic[i], 0.0f, 0.1f);
                                    ImGui::ColorEdit3(("Light " + std::to_string(i) + " Ambient").c_str(), &pointLightAmbient[i].x);
                                    ImGui::ColorEdit3(("Light " + std::to_string(i) + " Diffuse").c_str(), &pointLightDiffuse[i].x);
                                    ImGui::ColorEdit3(("Light " + std::to_string(i) + " Specular").c_str(), &pointLightSpecular[i].x);
                                }
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Flashlight"))
                            {
                                ImGui::Text("Flashlight Controls");
                                ImGui::Checkbox("Flashlight On", &flashlightOn);
                                // ImGui::ColorEdit3("Flashlight Colour", &spotlightColor.x);
                                // ImGui::SliderFloat("Flashlight Intensity", &spotlightIntensity, 0.0f, 5.0f);
                                ImGui::SliderFloat("Flashlight Cutoff", &flashlightCutoff, 1.0f, 90.0f);
                                ImGui::SliderFloat("Flashlight Outer Cutoff", &flashlightOuterCutoff, 1.0f, 90.0f);
                                ImGui::EndTabItem();
                            }
                        }
                        ImGui::EndTabBar();
                    }

                    ImGui::Separator();
                    if (ImGui::CollapsingHeader("Sky / Atmosphere")) {
                        ImGui::Text("Sky Controls");
                        ImGui::Checkbox("Pause Sun Animation", &skyTimePaused);
                        ImGui::SliderFloat("Sun Time Offset (s)", &skyTimeOffset, 0.0f, 30.0f, "%.1f");
                        ImGui::SliderFloat("Exposure", &skyExposure, 0.1f, 4.0f, "%.2f");
                        ImGui::SliderFloat("Atmos Density", &skyAtmDensity, 0.0f, 100.0f, "%.2f");
                        ImGui::SliderFloat("Atmos Thickness", &skyAtmThickness, 0.0f, 1.0f, "%.2f");
                        ImGui::SliderFloat("Planet Scale", &planetScale, 5000.0f, 15000.0f, "%.2f");
                        ImGui::TextDisabled("Lower density/thickness to feel higher altitude.");
                    }

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Settings"))
                {
                    if (ImGui::DragFloat("Dbg window Font Size", &style.FontSizeBase, 0.20f, 5.0f, 100.0f, "%.0f"))
                        style._NextFrameFontSizeBase = style.FontSizeBase; // FIXME: Temporary hack until we finish remaining work.
                    
                        ImGui::EndTabItem();
                }
                ImGui::EndTabBar();
            }
            
            
            ImGui::End();
            if (!uiInteractive) {
                ImGui::PopStyleVar();
            }
        }
}

void App::run() {
    init();
    loadResources();
    render();
}

void App::cleanup() {

    // Shutdown ImGui before terminating GLFW
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteBuffers(1, &EBO);
    glDeleteVertexArrays(1, &lightCubeVAO);
    glDeleteBuffers(1, &lightCubeVBO);
    glDeleteTextures(1, &texture);

	NetDisconnect pkt;
	pkt.username = "Steve";
	udpClient->sendPacket(pkt);

    glfwTerminate();
    saveControls();
}

void App::loadControlsDefaults() {
	controlsArray[FORWARD]				= GLFW_KEY_W;
	controlsArray[BACKWARD]        		= GLFW_KEY_S;
	controlsArray[LEFT]					= GLFW_KEY_A;
	controlsArray[RIGHT]				= GLFW_KEY_D;
    controlsArray[UP]					= GLFW_KEY_SPACE;
    controlsArray[DOWN]					= GLFW_KEY_LEFT_SHIFT;
    controlsArray[LEFT_CLICK]			= GLFW_MOUSE_BUTTON_LEFT;
    controlsArray[TOGGLE_FULLSCREEN]	= GLFW_KEY_F11;
    controlsArray[TOGGLE_WIREFRAME]		= GLFW_KEY_F1;
    controlsArray[TOGGLE_SHADER]		= GLFW_KEY_F2;
    controlsArray[TOGGLE_DEBUG]			= GLFW_KEY_TAB;
    controlsArray[MOVE_FAST]			= GLFW_KEY_LEFT_CONTROL;
    controlsArray[CLOSE_WINDOW]			= GLFW_KEY_ESCAPE;
}

void App::loadControlsFromFile(const char* filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        loadControlsDefaults();
        return;
    }

    // Initialize defaults first
    loadControlsDefaults();

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string keyName;
        int keyValue;
        if (!(iss >> keyName >> keyValue)) continue;

        for (int i = 0; i < CONTROL_COUNT; ++i) {
            if (keyName == controlNames[i]) {
                controlsArray[i] = keyValue;
                break;
            }
        }
    }
}

void App::saveControls(const char* filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return; // handle errors as you want

    for (int i = 0; i < CONTROL_COUNT; ++i) {
        file << controlNames[i] << " " << controlsArray[i] << "\n";
    }

	file << "\n\n# see 'https://www.glfw.org/docs/latest/group__keys.html' for key values" << '\n';
}

NetPlayerInputs App::buildPlayerInputsPacket()
{
	NetPlayerInputs inputs;

	//build keys;
	uint16_t keys = 0;
	if (glfwGetKey(window, controlsArray[FORWARD]) == GLFW_PRESS)
		keys |= IN_FORWARD;
	if (glfwGetKey(window, controlsArray[BACKWARD]) == GLFW_PRESS)
		keys |= IN_BACKWARD;
	if (glfwGetKey(window, controlsArray[LEFT]) == GLFW_PRESS)
		keys |= IN_LEFT;
	if (glfwGetKey(window, controlsArray[RIGHT]) == GLFW_PRESS)
		keys |= IN_RIGHT;

	if (glfwGetKey(window, controlsArray[UP]) == GLFW_PRESS)
		keys |= IN_UP;
	if (glfwGetKey(window, controlsArray[DOWN]) == GLFW_PRESS)
		keys |= IN_DOWN;

	if (glfwGetKey(window, controlsArray[MOVE_FAST]) == GLFW_PRESS)
		keys |= IN_RUN;

	inputs.keys = keys;
	inputs.pitch = camera->getPitch();
	inputs.yaw = camera->getYaw();
	inputs.loadRadius = camera->getLoadRadius();

	return inputs;
}

void App::processInput() {
    static bool f11Held = false;
    static bool f1Held  = false;
    static bool f2Held  = false;
    static bool f4Held  = false;
    static bool tabHeld = false;
    static bool leftMousePressedLastFrame = false;
	static bool rightMousePressedLastFrame = false;

	//reload chunk. F3 + A;
	if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS &&
    	glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
		for (auto &chunkPtr : renderer->getRenderedChunks())
		{
			if (auto chunk = chunkPtr.lock())
				chunk->buildMesh();
		}
		return;
	}

    // Show/Hide debug window
    if (glfwGetKey(window, controlsArray[TOGGLE_DEBUG]) == GLFW_PRESS && !tabHeld) {
        showDebugWindow = !showDebugWindow;
        tabHeld = true;

        if (!showDebugWindow && uiInteractive) {
            uiInteractive = false;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        }
    }
    if (glfwGetKey(window, controlsArray[TOGGLE_DEBUG]) == GLFW_RELEASE) {
        tabHeld = false;
    }

    // Toggle interactive mode with F4.  We debounce the key to avoid
    // multiple toggles per press.  When uiInteractive is true we release
    // the mouse and ImGui windows will capture input.  When false we
    // recapture the mouse and treat the debug window as an overlay only.
    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_PRESS && !f4Held) {
        uiInteractive = !uiInteractive;
        f4Held = true;
        if (uiInteractive) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        } else {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstMouse = true;
        }
    }
    if (glfwGetKey(window, GLFW_KEY_F4) == GLFW_RELEASE) {
        f4Held = false;
    }

    // Start by getting the ImGui IO structure.  We will respect its capture flags
    // when deciding whether to process game inputs.  Note: this call is valid
    // even if ImGui hasn’t been initialised in the current frame yet.
    ImGuiIO& io = ImGui::GetIO();

    // Determine whether game input should be suppressed.  When uiInteractive
    // is false the overlay is visible but non-interactive, so we never
    // suppress input.  When uiInteractive is true we honour ImGui’s capture
    // flags to decide whether to ignore keyboard or mouse events.
    const bool capturingKeyboard = uiInteractive && io.WantCaptureKeyboard;
    const bool capturingMouse    = uiInteractive && io.WantCaptureMouse;

    // Handle keyboard-based game actions when input isn’t captured.
    if (!capturingKeyboard) {
        // Toggle Fullscreen
        if (glfwGetKey(window, controlsArray[TOGGLE_FULLSCREEN]) == GLFW_PRESS && !f11Held) {
            toggleDisplayMode();
            f11Held = true;
        }
        if (glfwGetKey(window, controlsArray[TOGGLE_FULLSCREEN]) == GLFW_RELEASE) {
            f11Held = false;
        }

        // Toggle Wireframe Mode
        if (glfwGetKey(window, controlsArray[TOGGLE_WIREFRAME]) == GLFW_PRESS && !f1Held) {
            wireframe = !wireframe;
            glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
            f1Held = true;
        }
        if (glfwGetKey(window, controlsArray[TOGGLE_WIREFRAME]) == GLFW_RELEASE) {
            f1Held = false;
        }

        // Toggle Shader (switch between texture and gradient shader).  When
        // useGradientShader is true we use gradientShader; otherwise we use
        // textureShader.
        if (glfwGetKey(window, controlsArray[TOGGLE_SHADER]) == GLFW_PRESS && !f2Held) {
            useGradientShader = !useGradientShader;
            activeShader = useGradientShader ? gradientShader : textureShader;
            f2Held = true;
        }
        if (glfwGetKey(window, controlsArray[TOGGLE_SHADER]) == GLFW_RELEASE) {
            f2Held = false;
        }

    }

    // Exit (ESC).  Allow closing window even when ImGui doesn’t want keyboard.
    if (glfwGetKey(window, controlsArray[CLOSE_WINDOW]) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
}


void App::updateWindowTitle() {
    const float currentFrame = glfwGetTime();
    frameCount++;

    if (currentFrame - lastTitleUpdate >= 0.5f) {
        const float fps = frameCount / (currentFrame - lastTitleUpdate);
        const float msPerFrame = 1000.0f / fps;

        const std::string title = "OpenGL (ft_minecraft) - " +
            std::to_string(static_cast<int>(fps)) + " FPS / " +
            std::to_string(msPerFrame).substr(0, 5) + " ms";

        glfwSetWindowTitle(window, title.c_str());

        lastTitleUpdate = currentFrame;
        frameCount = 0;
    }

}

void App::toggleDisplayMode() {
    if (displayMode == DisplayMode::Windowed) {
        // Save windowed size and position
        glfwGetWindowPos(window, &windowedX, &windowedY);
        glfwGetWindowSize(window, &windowedWidth, &windowedHeight);

        // Go to fullscreen (exclusive)
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        displayMode = DisplayMode::Fullscreen;

    } else {
        // Back to windowed
        glfwSetWindowMonitor(window, nullptr, windowedX, windowedY, windowedWidth, windowedHeight, 0);
        glfwSetWindowAttrib(window, GLFW_DECORATED, GLFW_TRUE);
        displayMode = DisplayMode::Windowed;
    }
}

unsigned int App::loadTexture(const char* path) {
    GLuint texID;
    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    int w, h, ch;
    stbi_set_flip_vertically_on_load(true);
    unsigned char* data = stbi_load(path, &w, &h, &ch, 0);
    if (data) {
        const GLenum format = ch == 4 ? GL_RGBA : GL_RGB;
        glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
    } else {
        std::cerr << "Failed to load texture: " << path << "\n";
    }
    
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    
    stbi_image_free(data);
    
    return texID;
}

// static
// Returns the current process’s resident set size (RSS) in bytes.  On
// Linux this reads /proc/self/statm.  On other platforms this will
// return 0.  RSS is an approximation of the memory the process is
// currently using in RAM.
size_t App::getCurrentRSS() {
#ifdef __linux__
    long rss = 0L;
    long pageSize = 0L;
    FILE* fp = nullptr;
    fp = fopen("/proc/self/statm", "r");
    if (fp != nullptr) {
        /* Each entry in statm is a number of pages.  The second entry is
           the resident set size. */
        unsigned long dummy;
        if (fscanf(fp, "%lu %lu", &dummy, &rss) != 2) {
            rss = 0L;
        }
        fclose(fp);
    }
    pageSize = sysconf(_SC_PAGESIZE);
    return (size_t)rss * (size_t)pageSize;
#else
    return 0;
#endif
}
