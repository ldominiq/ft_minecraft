//
// Created by lucas on 6/25/25.
//

#include "App.hpp"

App::App():
			camera(nullptr),
			monitor(nullptr),
			mode(nullptr),

            lighting(nullptr),
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
    //glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
	glfwSetWindowUserPointer(window, this);

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, const int width, const int height) {
		App* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        glViewport(0, 0, width, height);
		glfwGetFramebufferSize(w, &app->screenWidth, &app->screenHeight);
		auto manager = app->menuManager.lock();
		if (manager)
			manager->resize(width, height);
		if (manager != app->inventoryUI)
			app->inventoryUI->resize(width, height);
		if (manager != app->chat)
			app->chat->resize(width, height);
    });

    glfwMakeContextCurrent(window);
    gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));

    glfwGetFramebufferSize(window, &windowedWidth, &windowedHeight);

	udpClient = std::make_unique<UDPClient>("127.0.0.1");
	setUdpClientPacketCallback();

	renderer = std::make_unique<Renderer>();

	// ********************Water Renderer setup******************************
	waterFramebuffer = std::make_shared<WaterFramebuffer>(windowedWidth, windowedHeight);
	waterShader = std::make_shared<Shader>("shaders/water.vert", "shaders/water.frag");
	waterRenderer = std::make_unique<WaterRenderer>(waterShader, waterFramebuffer);

	// ********************Chunk Boundary Renderer**************************
	chunkBoundaryRenderer = std::make_unique<ChunkBoundaryRenderer>();

	// ********************Render Type Debug Framebuffers********************
	renderTypeFramebuffer = std::make_unique<RenderTypeFramebuffer>(windowedWidth, windowedHeight);

	loader = std::make_unique<Loader>();
	// GUI textures are now dynamically managed based on debug flags
    guiRenderer = std::make_unique<GuiRenderer>(*loader);

    lighting = std::make_unique<Lighting>(windowedWidth, windowedHeight);

	chat = std::make_shared<Chat>(windowedWidth, windowedHeight);
	inventoryUI = std::make_shared<InventoryUI>(windowedWidth, windowedHeight, &textureManager);

	m_itemPropEntityManager = std::make_unique<ItemPropEntityManager>(&textureManager);

    gBuffer = std::make_shared<GBuffer>(windowedWidth, windowedHeight);
    ssao = std::make_shared<SSAO>(windowedWidth, windowedHeight);

    glEnable(GL_DEPTH_TEST);
    
    // enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    lighting->initCSMResources();


    // Mouse movement event handling
    camera = std::make_unique<Camera>(glm::vec3(0.0f, 128.0f, 0.0f));
    glfwSetCursorPosCallback(window, [](GLFWwindow* w, const double xpos, const double ypos) {
        static App* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        if (!app) return;
        // Honour ImGui’s mouse capture: if the UI is being interacted with
        // (e.g. hovering/clicking in a window), do not rotate the camera.
        ImGuiIO& io = ImGui::GetIO();

		auto menuManagerPtr = app->menuManager.lock();
		if (menuManagerPtr)
		{
			menuManagerPtr->handleMouseMove(xpos, ypos);
			return ;
		}

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

		app->mouseMovedRecently = true;
		app->lastMouseMoveTime = glfwGetTime();
    });
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int codepoint) {
		App* app = static_cast<App*>(glfwGetWindowUserPointer(w));
		if (!app) return;
		auto manager = app->menuManager.lock();
		if (manager != app->chat) return ;

		app->chat->addCharToCurrMsg(static_cast<char>(codepoint));
	});

	glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
		App* app = static_cast<App*>(glfwGetWindowUserPointer(w));
		if (!app) return;

		auto manager = app->menuManager.lock();

		if (!manager && app->controlsArray[CLOSE_WINDOW] == key && action == GLFW_PRESS) glfwSetWindowShouldClose(w, true);

		app->processInputMenus(key, action);
		if (manager) return ;

		auto mapKeyToBit = [](int key) -> uint16_t {
			switch (key) {
				case GLFW_KEY_W: return IN_FORWARD;
				case GLFW_KEY_S: return IN_BACKWARD;
				case GLFW_KEY_A: return IN_LEFT;
				case GLFW_KEY_D: return IN_RIGHT;
				case GLFW_KEY_SPACE: return IN_UP;   // jump
				case GLFW_KEY_LEFT_SHIFT: return IN_RUN;
				case GLFW_KEY_LEFT_CONTROL: return IN_DOWN;
				case GLFW_KEY_Q: return IN_DROP;
				default: return 0; // key not tracked
			}
		};

		bool hotbarUpdated = app->controlsArray[HOTBAR_1] == key ||
							app->controlsArray[HOTBAR_2] == key ||
							app->controlsArray[HOTBAR_3] == key ||
							app->controlsArray[HOTBAR_4] == key ||
							app->controlsArray[HOTBAR_5] == key ||
							app->controlsArray[HOTBAR_6] == key ||
							app->controlsArray[HOTBAR_7] == key ||
							app->controlsArray[HOTBAR_8] == key ||
							app->controlsArray[HOTBAR_9] == key;

		uint16_t bit = mapKeyToBit(key);
		if (!bit && !hotbarUpdated) return; // not an input we care about

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

		auto manager = app->menuManager.lock();
		if (manager)
		{
			double mouseX, mouseY;
    		glfwGetCursorPos(w, &mouseX, &mouseY);

			if (manager == app->inventoryUI)
			{
				manager->handleMouseClick(mouseX, mouseY, button, action);
				if (app->inventoryUI->lastAction.has_value())
				{
					auto [slot, type] = *app->inventoryUI->lastAction;
					NetInventoryAction pkt;
					pkt.actionType = type;
					pkt.slot = slot;
					app->udpClient->sendPacket(pkt);
					app->inventoryUI->lastAction.reset();
				}
			}
			return ;
		}

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

	renderer->livingEntitiesManager.add(camera->getPlayer());

    // Generate query pools
    glGenQueries(QUERY_POOL_SIZE, queryDrawSkyPool);
    glGenQueries(QUERY_POOL_SIZE, queryDrawCloudsPool);
    glGenQueries(QUERY_POOL_SIZE, queryDrawWaterReflectionPool);
    glGenQueries(QUERY_POOL_SIZE, queryDrawShadowsPool);
    glGenQueries(QUERY_POOL_SIZE, queryRenderShaderPool);
    glGenQueries(QUERY_POOL_SIZE, queryRenderWaterPool);
    glGenQueries(QUERY_POOL_SIZE, queryDrawEntities);
    glGenQueries(QUERY_POOL_SIZE, querySSAOPool);
}

void App::setUdpClientPacketCallback()
{
	udpClient->setCallback([this](const PacketPtr& pkt) {

		switch (pkt->type) {

			case PacketType::GROUP: {
				auto& g = static_cast<NetPacketGroup&>(*pkt);
				for (auto& inner : g.unpack()) {
					if (udpClient->onPacket)
						udpClient->onPacket({ std::move(inner) });
				}
				break;
			}

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
				// snapshotReceivedTime = glfwGetTime();
				camera->onSnapshot(p, *renderer, clientTick);
				break;
			}

			case PacketType::PLAYER_GAMEMODE: {
				auto& p = static_cast<NetPlayerGameMode&>(*pkt);
				camera->getPlayer()->gamemode = static_cast<GAMEMODES>(p.gamemode);
				break;
			}

			case PacketType::NET_ENTITY_MOVE: {
				auto& p = static_cast<NetEntityMove&>(*pkt);
				renderer->onEntity(p, clientTime);
				break;
			}

			case PacketType::NET_INVENTORY: {
				auto& p = static_cast<NetInventory&>(*pkt);
				inventoryUI->setSlot(p.slot, p.amount, p.type);
				break;
			}

			case PacketType::MODIFIED_BLOCK_DATA: {
				auto& p = static_cast<NetModifiedBlockData&>(*pkt);
				renderer->updateChunk(p);
				break;
			}

			case PacketType::NET_MESSAGE: {
				auto& p = static_cast<NetMessage&>(*pkt);
				chat->updateChatlog(p.message);
				break;
			}

            case PacketType::NET_IMGUI: {
                auto& p = static_cast<NetImGui&>(*pkt);
                // handle ImGui data (e.g., update UI state)
                currentBiome = p.currentBiome;
                break;
            }

			default:
				std::cout << "Unknown packet type: " << static_cast<int>(pkt->type) << "\n";
				break;
		}
	});
}


void App::loadResources() {
    // Load shaders and textures

    textureShader = std::make_shared<Shader>("shaders/lighting.vert", "shaders/lighting.frag");
    gradientShader = std::make_shared<Shader>("shaders/gradient.vert", "shaders/gradient.frag");

    // Load individual block textures into a texture array
    textureManager.loadResourcePack("assets");

    activeShader = textureShader;

    activeShader->use();
    activeShader->setInt("blockTextures", 0);

    // shader configuration
    // --------------------

	waterRenderer->setDependencies(lighting, renderer, camera);

    gBufferShader = std::make_shared<Shader>("shaders/ssao_geometry.vert", "shaders/ssao_geometry.frag");
    gBufferShader->use();
    gBufferShader->setInt("blockTextures", 0);

    // Wire the texture manager to subsystems that need it
    renderer->setTextureManager(&textureManager);
}

void App::render() {

	while (!glfwWindowShouldClose(window)) {
		// Rotate query index each frame
		currentQueryIndex = (currentQueryIndex + 1) % QUERY_POOL_SIZE;

		// Calculate delta time for frame rate
		currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		if (camera)
			camera->updateSmoothing(deltaTime);

		NetPlayerInputs inputs = buildPlayerInputsPacket();
		auto manager = menuManager.lock();

		//Tick logic
		float tickDuration = 1.0f / TPS; // 0.05s per tick
		static float accumulator = 0.0f;
		accumulator += deltaTime;

        int simulatedTicksThisFrame = 0;
        constexpr int kMaxSimulatedTicksPerFrame = 6;
        while (accumulator >= tickDuration && simulatedTicksThisFrame < kMaxSimulatedTicksPerFrame)
        {
            NetPlayerInputs tickInputs = inputs;
            if (manager)
                tickInputs.keys = 0;

            tickInputs.serverClientReconciliationTick = clientTick;
            camera->queueInput(tickInputs, clientTick);
            udpClient->sendPacket(tickInputs);
            camera->predict(*renderer, clientTick);

            clientTime = clientTick * tickDuration;
            accumulator -= tickDuration;
            clientTickChangedTime = glfwGetTime();
            clientTick++;
            simulatedTicksThisFrame++;
        }

        if (simulatedTicksThisFrame == kMaxSimulatedTicksPerFrame && accumulator > tickDuration * 2.0f)
            accumulator = tickDuration * 2.0f;

        camera->setRenderTickAlpha(accumulator / tickDuration);
		udpClient->receivePacket();
        camera->flushPendingSnapshot(*renderer, clientTick);

		//for some reason mouse needs a little delay to be put to false otherwise it glitches.
		if (lastMouseMoveTime > glfwGetTime() + tickDuration * 2)
			mouseMovedRecently = false;

     // Local player must be rendered from current predicted state (present time), not interpolated in the past.

		// const double mouseIdleThreshold = 0.2; // seconds
		// if (mouseMovedRecently && (glfwGetTime() - lastMouseMoveTime) > mouseIdleThreshold)
		// 	mouseMovedRecently = false;

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

		// auto manager = menuManager.lock();
		if (manager != chat)
        	processInput();

        // window aspect / uniforms
        const float aspect = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);

        glm::mat4 view = camera->getViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(80.0f), aspect, 0.1f, renderDistance);
		glm::vec4 clipPlane = glm::vec4(0, -1, 0, 100000);  // No clipping

        // Update camera frustum for chunk culling (once per frame, before any render call)
        renderer->updateFrustum(projection * view);


        lighting->setViewportSize(screenWidth, screenHeight);
        lighting->updateSunDirection(deltaTime);
        lighting->updateSkyLUT(camera->getPlayer()->getPosition().y);


        if (lighting->isShadowsEnabled() && lighting->isSunAboveHorizon()) {
            glBeginQuery(GL_TIME_ELAPSED, queryDrawShadowsPool[currentQueryIndex]);

            lighting->updateCSMShadowMaps(*renderer, view, textureManager);

            glEndQuery(GL_TIME_ELAPSED);
            shadowQueryIssuedThisFrame[currentQueryIndex] = true;
        } else {
            shadowQueryIssuedThisFrame[currentQueryIndex] = false;
        }

        // Render to debug framebuffers if enabled
        if (showNormalsTexture) {
            renderTypeFramebuffer->bindNormalsFrameBuffer();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            textureShader->use();
            textureShader->setInt("renderType", 1); // Normals mode
            textureShader->setVec4("clipPlane", clipPlane);
            textureShader->setMat4("view", view);
            textureShader->setMat4("projection", projection);
            lighting->uploadLightingUniforms(*textureShader, camera->getPlayer()->getPosition(), camera->getPlayer()->getCameraDir());
            glActiveTexture(GL_TEXTURE0);
            textureManager.bind(GL_TEXTURE0);
            renderer->render(textureShader);
            renderTypeFramebuffer->unbindCurrentFrameBuffer();
        }

        if (showDepthTexture) {
            renderTypeFramebuffer->bindDepthFrameBuffer();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            textureShader->use();
            textureShader->setInt("renderType", 2); // Depth mode
            textureShader->setVec4("clipPlane", clipPlane);
            textureShader->setMat4("view", view);
            textureShader->setMat4("projection", projection);
            lighting->uploadLightingUniforms(*textureShader, camera->getPlayer()->getPosition(), camera->getPlayer()->getCameraDir());
            glActiveTexture(GL_TEXTURE0);
            textureManager.bind(GL_TEXTURE0);
            renderer->render(textureShader);
            renderTypeFramebuffer->unbindCurrentFrameBuffer();
        }

        // Restore main renderType for normal scene rendering
        if (textureShader) {
            textureShader->use();
            textureShader->setInt("renderType", selectedRenderType); // Normal lighting mode
        }

        // GBuffer pass
        if (ssao && ssao->isEnabled()) {
            gBuffer->resize(screenWidth, screenHeight);
            ssao->resize(screenWidth, screenHeight);

            gBuffer->bind();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            gBufferShader->use();
            gBufferShader->setMat4("view", view);
            gBufferShader->setMat4("projection", projection);
            textureManager.bind(GL_TEXTURE0);
            renderer->render(gBufferShader);

            gBuffer->unbind();

            // SSAO pass
            glBeginQuery(GL_TIME_ELAPSED, querySSAOPool[currentQueryIndex]);

            ssao->renderSSAO(*gBuffer, projection);
            if (ssao->isBlurEnabled())
                ssao->blurSSAO();

            glEndQuery(GL_TIME_ELAPSED);
            ssaoQueryIssuedThisFrame[currentQueryIndex] = true;

            // Restore full-res viewport (SSAO may have rendered at half resolution)
            glViewport(0, 0, screenWidth, screenHeight);

        } else {
            ssaoQueryIssuedThisFrame[currentQueryIndex] = false;
        }

		static float waterMoveOffset = waterRenderer->getWaterMoveFactor();
		static float waveSpeed = waterRenderer->waveStrength;
		waterMoveOffset += waveSpeed * deltaTime;
		if (waterMoveOffset > 1.0f) waterMoveOffset = 0.0f;
		waterRenderer->setWaterMoveFactor(waterMoveOffset);

        glBeginQuery(GL_TIME_ELAPSED, queryDrawWaterReflectionPool[currentQueryIndex]);
        
        // Render reflection texture
    	waterRenderer->renderWaterReflectionPass(activeShader, projection, textureManager);

        glEndQuery(GL_TIME_ELAPSED);


    	// render refraction texture
    	waterRenderer->renderWaterRefractionPass(activeShader, view, projection, textureManager);

    	// render to screen
    	renderScene(view, projection, clipPlane);
    	
    	// Render water with proper shader setup
        glBeginQuery(GL_TIME_ELAPSED, queryRenderWaterPool[currentQueryIndex]);
    	waterRenderer->renderWaterSurface(projection);
        glEndQuery(GL_TIME_ELAPSED);

		const int currentChunkX = static_cast<int>(std::floor(camera->getPlayer()->getPosition().x / Chunk::WIDTH));
		const int currentChunkZ = static_cast<int>(std::floor(camera->getPlayer()->getPosition().z / Chunk::DEPTH));

		renderer->buildChunks();
		renderer->organizeChunks(Chunk::toKey(currentChunkX, currentChunkZ), camera->getPlayer()->getLoadRadius());
        camera->drawWireframeSelectedBlockFace(renderer, view, projection);

        // Draw chunk boundary overlay (if enabled)
        chunkBoundaryRenderer->draw(camera->getPlayer()->getPosition(), view, projection, *renderer);

        glBindVertexArray(0);
        {
    		// Dynamically build GUI textures based on debug flags
    		guis.clear();
    		if (showReflectionTexture) {
    			guis.emplace_back(waterFramebuffer->getReflectionTexture(), glm::vec2(0.48f, 0.75f), glm::vec2(0.2f, 0.2f));
    		}
    		if (showRefractionTexture) {
    			guis.emplace_back(waterFramebuffer->getRefractionTexture(), glm::vec2(0.48f, 0.3f), glm::vec2(0.2f, 0.2f), true);
    		}
    		if (showRefractionDepthTexture) {
    			guis.emplace_back(waterFramebuffer->getRefractionDepthTexture(), glm::vec2(0.48f, -0.15f), glm::vec2(0.2f, 0.2f), true, true);
    		}
    		if (showNormalsTexture && renderTypeFramebuffer) {
    			guis.emplace_back(renderTypeFramebuffer->getNormalsTexture(), glm::vec2(0.05f, 0.75f), glm::vec2(0.2f, 0.2f), true);
    		}
    		if (showDepthTexture && renderTypeFramebuffer) {
    			guis.emplace_back(renderTypeFramebuffer->getDepthTexture(), glm::vec2(0.05f, 0.3f), glm::vec2(0.2f, 0.2f), true);
    		}

    		guiRenderer->render(guis, 0.1f, renderDistance);
        }

        if (showDebugWindow) {
            debugWindow();
        }

    	if (lighting->isShadowMapEnabled())
    		lighting->drawCSMShadowMapPreview(lighting->debugPreviewLayer);

    	if (showSSAOTexture && ssao && ssao->isEnabled())
    		lighting->drawTexturePreviewQuad(ssao->getSSAOTexture(), true, glm::vec2(0.0f, 0.2f));

    	if (showSSAORawTexture && ssao && ssao->isEnabled())
    		lighting->drawTexturePreviewQuad(ssao->getRawSSAOTexture(), true, glm::vec2(0.42f, 0.2f));

    	if (showGBufferPositionTexture && gBuffer)
    		lighting->drawTexturePreviewQuad(gBuffer->getPositionTexture(), false, glm::vec2(0.84f, 0.2f));

    	if (showGBufferNormalTexture && gBuffer)
    		lighting->drawTexturePreviewQuad(gBuffer->getNormalTexture(), false, glm::vec2(1.26f, 0.2f));

    	if (!uiInteractive && (lighting->showCSMDebugView || showFrustumCullingDebug))
    		ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);

    	if (lighting->showCSMDebugView)
    		lighting->drawCSMDebugView(
    			camera->getPlayer()->getPosition(),
    			camera->getPlayer()->getCameraDir(),
    			view);

        //TODO: pass FOV and near plane from actual camera settings instead of hardcoding
    	if (showFrustumCullingDebug)
    		renderer->drawFrustumCullingDebug(
    			camera->getPlayer()->getPosition(),
    			camera->getPlayer()->getCameraDir(),
    			80.0f, aspect, 0.1f, renderDistance);

    	if (!uiInteractive && (lighting->showCSMDebugView || showFrustumCullingDebug))
    		ImGui::PopStyleVar();

        // Finalize ImGui rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

		// render menus last
		if (manager) {
			manager->render();
		}
		else
		{
			inventoryUI->drawHotbar();
			inventoryUI->drawHealth(camera->getPlayer()->health);
			chat->renderRecentMessages();
		}

        // Swap buffers and poll events (keys pressed, mouse movement, etc.)
        glfwSwapBuffers(window);
        glfwPollEvents();

        if (profilingEnabled) {
            // Read results from 2 frames ago to give the GPU time to finish
            int readIndex = (currentQueryIndex + QUERY_POOL_SIZE - 2) % QUERY_POOL_SIZE;
            float a = profilingEMASmoothing;

            readGPUQueryEMA(queryDrawSkyPool[readIndex], measuredAverageMsDrawSky, a);
            readGPUQueryEMA(queryDrawCloudsPool[readIndex], measuredAverageMsDrawClouds, a);
            readGPUQueryEMA(queryDrawWaterReflectionPool[readIndex], measuredAverageMsDrawWaterReflection, a);
            readGPUQueryEMA(queryRenderShaderPool[readIndex], measuredAverageMsRenderShader, a);
            readGPUQueryEMA(queryRenderWaterPool[readIndex], measuredAverageMsRenderWater, a);
            readGPUQueryEMA(queryDrawEntities[readIndex], measuredAverageMsDrawEntities, a);

            // SSAO: only read if the query was actually issued that frame.
            // Otherwise smoothly decay toward 0 so the display reflects reality.
            if (ssaoQueryIssuedThisFrame[readIndex]) {
                readGPUQueryEMA(querySSAOPool[readIndex], measuredAverageMsSSAO, a);
            } else {
                measuredAverageMsSSAO *= (1.0 - a);
            }


            // Shadows: only read if the query was actually issued that frame.
            // Otherwise smoothly decay toward 0 so the display reflects reality.
            if (shadowQueryIssuedThisFrame[readIndex]) {
                readGPUQueryEMA(queryDrawShadowsPool[readIndex], measuredAverageMsDrawShadows, a);
            } else {
                // Decay toward 0 when shadows are not being rendered
                measuredAverageMsDrawShadows *= (1.0 - a);
            }
        }
    }
}

bool readGPUQueryEMA(GLuint queryId, double &smoothedMs, float alpha)
{
    // Check if result is available (non-blocking)
    GLint available = 0;
    glGetQueryObjectiv(queryId, GL_QUERY_RESULT_AVAILABLE, &available);
    if (!available) return false;

    GLuint64 elapsed = 0;
    glGetQueryObjectui64v(queryId, GL_QUERY_RESULT, &elapsed);

    double sampleMs = static_cast<double>(elapsed) * 1.0e-6;

    // Exponential moving average: smoothed = alpha * sample + (1 - alpha) * smoothed
    // On first sample (smoothedMs == 0), just use the raw value
    if (smoothedMs <= 0.0)
        smoothedMs = sampleMs;
    else
        smoothedMs = alpha * sampleMs + (1.0 - alpha) * smoothedMs;

    return true;
}

void App::renderScene(glm::mat4 view, glm::mat4 projection, glm::vec4 clipPlane) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render sky/clouds first with proper depth
    glDisable(GL_CLIP_DISTANCE0);

    glBeginQuery(GL_TIME_ELAPSED, queryDrawCloudsPool[currentQueryIndex]);
    lighting->renderCloudsLowRes(view, projection, camera->getPlayer()->getPosition());
    glEndQuery(GL_TIME_ELAPSED);

    glBeginQuery(GL_TIME_ELAPSED, queryDrawSkyPool[currentQueryIndex]);
    lighting->drawSky(view, projection, camera->getPlayer()->getPosition());
    glEndQuery(GL_TIME_ELAPSED);

    // Now render terrain with depth testing enabled
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_CLIP_DISTANCE0);

    // Render solid blocks
    activeShader->use();
    activeShader->setVec4("clipPlane", clipPlane);
    activeShader->setMat4("view", view);
    activeShader->setMat4("projection", projection);
    lighting->uploadLightingUniforms(*activeShader, camera->getPlayer()->getPosition(), camera->getPlayer()->getCameraDir());
    lighting->uploadCSMUniforms(*activeShader, view);

    // Bind SSAO texture for the lighting shader (must be after activeShader->use())
    if (ssao && ssao->isEnabled()) {
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_2D, ssao->getSSAOTexture());
        activeShader->setInt("ssaoTexture", 5);
        activeShader->setInt("ssaoEnabled", 1);
        activeShader->setVec2("screenSize", glm::vec2(screenWidth, screenHeight));
    } else {
        activeShader->setInt("ssaoEnabled", 0);
    }

    glActiveTexture(GL_TEXTURE0);
    textureManager.bind(GL_TEXTURE0);

    glBeginQuery(GL_TIME_ELAPSED, queryRenderShaderPool[currentQueryIndex]);
    renderer->render(activeShader);
    glEndQuery(GL_TIME_ELAPSED);

    lighting->drawLightCubes(view, projection);

	// Check if the entity is within the player's load radius
	auto updateDrawState = [&](auto &entity)
	{
		glm::vec3 playerPos = camera->getPlayer()->getPosition();
		glm::vec3 entityPos = entity->getPosition();

		glm::vec2 diff(playerPos.x - entityPos.x, playerPos.z - entityPos.z);

		float distSq = glm::dot(diff, diff);
		float radius = camera->getPlayer()->getLoadRadius() * Chunk::WIDTH;

		entity->setDoDraw(distSq <= radius * radius);
	};

	//this code is mehhhh
	double intraTick = (glfwGetTime() - clientTickChangedTime);
	double delay = (1.0/TPS) * 1;
	//items
	for (auto &entity : renderer->itemEntities)
	{
		if (!entity->positionUpdated) continue ;
		entity->lerp(clientTime + intraTick - delay);
	}
    glBeginQuery(GL_TIME_ELAPSED, queryDrawEntities[currentQueryIndex]);
	m_itemPropEntityManager->draw(projection, view, renderer->itemEntities);
	glEndQuery(GL_TIME_ELAPSED);

	//mobs
	for (auto &entity : renderer->livingEntities)
	{
		updateDrawState(entity);

		entity->lerp(clientTime + intraTick - delay);

		// firstFrame = false;
	}

	for (auto &entity : renderer->itemEntities)
	{
		updateDrawState(entity);

		static bool firstFrame = true;
		if (!entity->snapshots.empty() && entity->getPosition() == entity->snapshots.back().position && !firstFrame) { 
            entity->positionUpdated = false; 
        }
		firstFrame = false;
	}

	// Sync the local player's mesh position with the interpolated camera target
	// so the character doesn't shake in third-person due to the prediction/
	// reconciliation cycle updating the raw physics position mid-frame.
	auto &localPlayer = *camera->getPlayer();
	if (camera->isThirdPersonCameraActive()) {
		localPlayer.renderPos = camera->getInterpolatedPlayerPos();
		localPlayer.hasRenderPos = true;
	} else {
		localPlayer.hasRenderPos = false;
	}

	renderer->drawCharacters(projection, view, deltaTime);
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
                style._NextFrameFontSizeBase = 30.0f;
                appliedDefaultFontSize = true;
            }

            glm::vec3 pos = camera->getPlayer()->getPosition();
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

					ImGui::Text("Player YAW: %f", camera->getPlayer()->yaw);

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
                const size_t totalChunks   = renderer->getTotalChunkCount();
                ImGui::Text("Chunks: %zu visible / %zu total", visibleChunks, totalChunks);

                size_t solidVertices = 0;
                size_t waterVertices = 0;
                for (auto& weakChunk : renderer->getRenderedChunks()) {
                    if (auto chunk = weakChunk.lock()) {
                        solidVertices += chunk->getMeshVerticesSize() / 10;
                        waterVertices += chunk->getWaterMeshVerticesSize() / 10;
                    }
                }
                
                size_t totalVertices = solidVertices + waterVertices;
                size_t totalTriangles = totalVertices / 3;
                size_t approximateBlocks = totalTriangles / 12;  // Each block can have up to 6 faces, 2 triangles per face
                
                // TODO: fix real count based on frustum culling
                ImGui::Text("Vertices: %zu solid + %zu water = %zu total", solidVertices, waterVertices, totalVertices);
                ImGui::Text("Triangles: %zu", totalTriangles);
                ImGui::Text("Approx. Visible Blocks: %zu", approximateBlocks);
            }

            // Display memory usage in megabytes.  We call a static helper to
            // obtain the current resident set size (RSS).
            {
                const size_t memBytes = getCurrentRSS();
                const double memMB = memBytes / (1024.0 * 1024.0);
                ImGui::Text("Memory: %.2f MB", memMB);
            }


                    ImGui::Separator();

                    // if (ImGui::CollapsingHeader("Teleportation")) {
                    //     // Teleport player
                    //     ImGui::Text("Teleport Player");
                    //     static float tmpX = 0;
                    //     static float tmpY = 100;
                    //     static float tmpZ = 0;
                    //     ImGui::InputFloat("X", &tmpX);
                    //     ImGui::InputFloat("Y", &tmpY);
                    //     ImGui::InputFloat("Z", &tmpZ);
                    //     if (ImGui::Button("Teleport")) {
                    //         camera->getPlayer()->setPosition(glm::vec3(tmpX, tmpY, tmpZ));
                    //     }
                    // }

                    // ImGui::Separator();

                    // Need to expose terrainParams from the server to the client..
                    // ImGui::Checkbox("Debug: Ores Only", &terrainParams.debugOresOnly);

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


                    // ImGui::Separator();

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

                    // ImGui::Separator();

                    if (ImGui::CollapsingHeader("Rendering")) {
                        if (ImGui::BeginTabBar("Rendering", tab_bar_flags))
                        {
                            if (ImGui::BeginTabItem("Options"))
                            {
                                if (ImGui::Checkbox("V-Sync", &vsync)) {
                                    glfwSwapInterval(vsync ? 1 : 0);
                                }
                                // Wireframe toggle
                                if (ImGui::Checkbox("Wireframe", &wireframe)) {
                                    glPolygonMode(GL_FRONT_AND_BACK, wireframe ? GL_LINE : GL_FILL);
                                }
                                // Shader toggle (texture vs gradient).  We update activeShader accordingly.
                                if (ImGui::Checkbox("Use Gradient Shader", &useGradientShader)) {
                                    activeShader = useGradientShader ? gradientShader : textureShader;
                                }
                                ImGui::RadioButton("Lighting render", &selectedRenderType, 0); ImGui::SameLine();
                                ImGui::RadioButton("Normals render",  &selectedRenderType, 1); ImGui::SameLine();
                                ImGui::RadioButton("Depth render",    &selectedRenderType, 2);

                                // Ensure the uniform is applied to the intended program(s),
                                // not whatever was last bound (e.g., selected-face wireframe).
                                auto applyRenderType = [&](const std::shared_ptr<Shader>& s) {
                                    if (!s) return;
                                    s->use();
                                    s->setInt("renderType", selectedRenderType);
                                };
                                applyRenderType(textureShader);

                                static bool useBlinnPhong = true;
                                if (ImGui::Checkbox("Blinn-Phong", &useBlinnPhong)) {
                                    textureShader->use();
                                    textureShader->setInt("blinn", useBlinnPhong);
                                }
                                bool shadowsEnabled = lighting->isShadowsEnabled();
                                if (ImGui::Checkbox("Shadows", &shadowsEnabled))
                                    lighting->setShadowsEnabled(shadowsEnabled);

                                bool ssaoEnabled = ssao->isEnabled();
                                if (ImGui::Checkbox("SSAO", &ssaoEnabled))
                                    ssao->setEnabled(ssaoEnabled);

                                // Changing this will update the far clipping plane.
                                ImGui::SliderFloat("Clipping plane Distance", &renderDistance, 100.0f, 2000.0f);

								// Adjust the chunk loading radius.  Casting to int and back avoids
								// accidental type issues in the setter.  We clamp the range to a
								// reasonable minimum and maximum.
								if (renderer) {
									int radius = camera->getPlayer()->getLoadRadius();
									if (ImGui::SliderInt("Chunk Load Radius", &radius, 4, 32)) {
										camera->getPlayer()->setLoadRadius(radius);
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

                                // Chunk boundary viewer
                                {
                                    bool cb = chunkBoundaryRenderer->isEnabled();
                                    if (ImGui::Checkbox("Show Chunk Boundary", &cb))
                                        chunkBoundaryRenderer->setEnabled(cb);
                                }

                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("SSAO"))
                            {
                                bool ssaoEnabled = ssao->isEnabled();
                                if (ImGui::Checkbox("Enable SSAO", &ssaoEnabled))
                                    ssao->setEnabled(ssaoEnabled);

                                bool blurEnabled = ssao->isBlurEnabled();
                                if (ImGui::Checkbox("Enable Blur", &blurEnabled))
                                    ssao->setBlurEnabled(blurEnabled);

                                bool halfRes = ssao->isHalfResolution();
                                if (ImGui::Checkbox("Half Resolution", &halfRes))
                                    ssao->setHalfResolution(halfRes);
                                ImGui::SameLine();
                                ImGui::TextDisabled("(?)");
                                if (ImGui::IsItemHovered())
                                    ImGui::SetTooltip("Render SSAO at half resolution");

                                ImGui::Separator();
                                ImGui::Text("Parameters");

                                float radius = ssao->getRadius();
                                if (ImGui::SliderFloat("Radius", &radius, 0.01f, 5.0f, "%.3f"))
                                    ssao->setRadius(radius);

                                float bias = ssao->getBias();
                                if (ImGui::SliderFloat("Bias", &bias, 0.0f, 0.2f, "%.4f"))
                                    ssao->setBias(bias);

                                float power = ssao->getPower();
                                if (ImGui::SliderFloat("Power", &power, 0.1f, 10.0f, "%.2f"))
                                    ssao->setPower(power);

                                int kernelSize = ssao->getKernelSize();
                                if (ImGui::SliderInt("Kernel Size", &kernelSize, 4, 64))
                                    ssao->setKernelSize(kernelSize);

                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Framebuffers"))
                            {
                                ImGui::Separator();
                                ImGui::Text("Water");
                                ImGui::Checkbox("Show Reflection Texture", &showReflectionTexture);
                                ImGui::Checkbox("Show Refraction Texture", &showRefractionTexture);
                                ImGui::Checkbox("Show Refraction Depth", &showRefractionDepthTexture);
                                
                                ImGui::Separator();
                                ImGui::Text("Render Type");
                                ImGui::Checkbox("Show Normals View", &showNormalsTexture);
                                ImGui::Checkbox("Show Depth View", &showDepthTexture);

                                ImGui::Separator();
                                ImGui::Text("SSAO");
                                ImGui::Checkbox("Preview SSAO Texture", &showSSAOTexture);
                                ImGui::Checkbox("Preview Raw SSAO (No Blur)", &showSSAORawTexture);
                                ImGui::Checkbox("Preview GBuffer Position", &showGBufferPositionTexture);
                                ImGui::Checkbox("Preview GBuffer Normal", &showGBufferNormalTexture);

                                ImGui::EndTabItem();
                            }
                        }
                        ImGui::EndTabBar();
                    }

                    // Lighting controls: direction and colours.  The direction vector
                    // components are clamped to [-1,1]; colours use a colour picker.
                    ImGui::Separator();
                    if (ImGui::CollapsingHeader("Lighting")) {
                        if (ImGui::BeginTabBar("Lighting", tab_bar_flags))
                        {
                            if (ImGui::BeginTabItem("Shadows"))
                            {
                                float shadowMinBias = lighting->getShadowMapMinBias();
                                float shadowMaxBias = lighting->getShadowMapMaxBias();

                            	ImGui::Text("Shadow Controls");
                                ImGui::Checkbox("Debug Cascades", &lighting->debugCascades);
                                ImGui::Checkbox("CSM Debug View (All Cascades)", &lighting->showCSMDebugView);
                                ImGui::Checkbox("Frustum Culling Radar", &showFrustumCullingDebug);
                                {
                                    bool fc = renderer->isFrustumCullingEnabled();
                                    if (ImGui::Checkbox("Enable Frustum Culling", &fc))
                                        renderer->setFrustumCullingEnabled(fc);
                                }

                                if (ImGui::SliderFloat("Shadow min Bias", &shadowMinBias, -0.005f, 0.001f, "%.5f"))
                                	lighting->setShadowMapMinBias(shadowMinBias);
                                if (ImGui::SliderFloat("Shadow max Bias", &shadowMaxBias, -0.005f, 0.005f, "%.5f"))
                                	lighting->setShadowMapMaxBias(shadowMaxBias);

                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Directional Light"))
                            {
                            	bool directionalLightOn = lighting->isDirectionalLightOn();
                            	glm::vec3 directionalLightDir = lighting->getDirectionalLightDirection();
                            	glm::vec3 directionalDiffuseColor = lighting->getDirectionalDiffuseColor();
                            	glm::vec3 directionalAmbientColor = lighting->getDirectionalAmbientColor();
                            	glm::vec3 directionalSpecularColor = lighting->getDirectionalSpecularColor();
                            	float materialShininess = lighting->getMaterialShininess();

                                ImGui::Text("Directional Light Controls");
                                if (ImGui::Checkbox("Light On", &directionalLightOn))
                                	lighting->setDirectionalLightEnabled(directionalLightOn);
                                if (ImGui::SliderFloat3("Light Direction", &directionalLightDir.x, -1.0f, 1.0f))
                                	lighting->setDirectionalLightDirection(directionalLightDir);
                                if (ImGui::ColorEdit3("Light Colour", &directionalDiffuseColor.x))
                                	lighting->setDirectionalDiffuseColor(directionalDiffuseColor);
                                if (ImGui::ColorEdit3("Ambient Colour", &directionalAmbientColor.x))
                                	lighting->setDirectionalAmbientColor(directionalAmbientColor);
                                if (ImGui::ColorEdit3("Specular Colour", &directionalSpecularColor.x))
                                	lighting->setDirectionalSpecularColor(directionalSpecularColor);
                                if (ImGui::SliderFloat("Material Shininess", &materialShininess, 1.0f, 256.0f))
                                	lighting->setMaterialShininess(materialShininess);
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Point Lights"))
                            {
                                ImGui::Text("Point Light Controls");
                                for (int i = 0; i < lighting->getNumPointLights(); ++i)
                                {
                                    bool enabled = lighting->isPointLightOn(i);
                                	glm::vec3 pointLightPosition = lighting->getPointLightPosition(i);
                                	glm::vec3 pointLightAmbient = lighting->getPointLightAmbient(i);
                                	glm::vec3 pointLightDiffuse = lighting->getPointLightDiffuse(i);
                                	glm::vec3 pointLightSpecular = lighting->getPointLightSpecular(i);
                                	float pointLightConstant = lighting->getPointLightConstant(i);
                                	float pointLightLinear = lighting->getPointLightLinear(i);
                                	float pointLightQuadratic = lighting->getPointLightQuadratic(i);

                                    if (ImGui::Checkbox(("Light " + std::to_string(i)).c_str(), &enabled))
                                        lighting->setPointLightEnabled(i, enabled);
                                    if (ImGui::SliderFloat3(("Light " + std::to_string(i) + " Position").c_str(), &pointLightPosition.x, 0.0f, 90.0f))
                                    	lighting->setPointLightPosition(i, pointLightPosition);
                                    if (ImGui::SliderFloat(("Light " + std::to_string(i) + " Constant").c_str(), &pointLightConstant, 0.0f, 2.0f))
                                    	lighting->setPointLightConstant(i, pointLightConstant);
                                    if (ImGui::SliderFloat(("Light " + std::to_string(i) + " Linear").c_str(), &pointLightLinear, 0.0f, 0.2f))
                                    	lighting->setPointLightLinear(i, pointLightLinear);
                                    if (ImGui::SliderFloat(("Light " + std::to_string(i) + " Quadratic").c_str(), &pointLightQuadratic, 0.0f, 0.1f))
                                    	lighting->setPointLightQuadratic(i, pointLightQuadratic);
                                    if (ImGui::ColorEdit3(("Light " + std::to_string(i) + " Ambient").c_str(), &pointLightAmbient.x))
                                    	lighting->setPointLightAmbient(i, pointLightAmbient);
                                    if (ImGui::ColorEdit3(("Light " + std::to_string(i) + " Diffuse").c_str(), &pointLightDiffuse.x))
                                    	lighting->setPointLightDiffuse(i, pointLightDiffuse);
                                    if (ImGui::ColorEdit3(("Light " + std::to_string(i) + " Specular").c_str(), &pointLightSpecular.x))
                                    	lighting->setPointLightSpecular(i, pointLightSpecular);
                                }
                                ImGui::EndTabItem();
                            }
                            if (ImGui::BeginTabItem("Flashlight"))
                            {
                            	bool flashlightOn = lighting->isFlashlightOn();
                            	float flashlightCutoff = lighting->getFlashlightCutoffAngle();
                            	float flashlightOuterCutoff = lighting->getFlashlightOuterCutoffAngle();

                                ImGui::Text("Flashlight Controls");
                                if (ImGui::Checkbox("Flashlight On", &flashlightOn))
                                	lighting->setSpotLightOn(flashlightOn);
                                // ImGui::ColorEdit3("Flashlight Colour", &spotlightColor.x);
                                // ImGui::SliderFloat("Flashlight Intensity", &spotlightIntensity, 0.0f, 5.0f);
                                if (ImGui::SliderFloat("Flashlight Cutoff", &flashlightCutoff, 1.0f, 90.0f))
									lighting->setFlashlightCutoffAngle(flashlightCutoff);
                                if (ImGui::SliderFloat("Flashlight Outer Cutoff", &flashlightOuterCutoff, 1.0f, 90.0f))
                                	lighting->setFlashlightOuterCutoffAngle(flashlightOuterCutoff);
                                ImGui::EndTabItem();
                            }
                        }
                        ImGui::EndTabBar();
                    }

                    ImGui::Separator();
                    if (ImGui::CollapsingHeader("Sky / Atmosphere")) {
                        bool skyTimePaused = lighting->isSkyTimePaused();
                    	float skyTimeOffset = lighting->getSkyTimeOffset();
                    	float sunYawDeg = lighting->getSunYawDeg();
                    	float skyExposure = lighting->getSkyExposure();
                    	float skyAtmDensity = lighting->getSkyAtmDensity();
                    	float skyAtmThickness = lighting->getSkyAtmThickness();
                    	float planetScale = lighting->getPlanetScale();

                        bool cloudsEnabled = lighting->isCloudsEnabled();
                        float cloudDensity = lighting->getCloudDensity();
                        float cloudSigmaT = lighting->getCloudSigmaT();
                        glm::vec3 cloudAlbedo = lighting->getCloudAlbedo();
                        float cloudStepCount = lighting->getCloudStepCount();
                        float cloudSigmaS = lighting->getCloudSigmaS();
                        float cloudPhaseG = lighting->getCloudPhaseG();

                        
                        if (ImGui::BeginTabBar("Sky / Atmosphere", tab_bar_flags))
                        {
                            if (ImGui::BeginTabItem("Atmosphere controls"))
                            {
                                bool skyLUTEnabled = lighting->isSkyLUTEnabled();
                                if (ImGui::Checkbox("Use Precomputed LUT (fast)", &skyLUTEnabled))
                                    lighting->setSkyLUTEnabled(skyLUTEnabled);
                                if (ImGui::Checkbox("Pause Sun Animation", &skyTimePaused))
                                    lighting->setSkyTimePaused(skyTimePaused);
                                if (ImGui::SliderFloat("Sun Time Offset (s)", &skyTimeOffset, 0.0f, 30.0f, "%.1f"))
                                    lighting->setSkyTimeOffset(skyTimeOffset);
                                if (ImGui::SliderFloat("Sun Yaw (degrees)", &sunYawDeg, 0.0f, 360.0f, "%.1f"))
                                    lighting->setSunYawDeg(sunYawDeg);
                                if (ImGui::SliderFloat("Exposure", &skyExposure, 0.1f, 4.0f, "%.2f"))
                                    lighting->setSkyExposure(skyExposure);
                                if (ImGui::SliderFloat("Atmos Density", &skyAtmDensity, 0.0f, 100.0f, "%.2f"))
                                    lighting->setSkyAtmDensity(skyAtmDensity);
                                if (ImGui::SliderFloat("Atmos Thickness", &skyAtmThickness, 0.0f, 1.0f, "%.2f"))
                                    lighting->setSkyAtmThickness(skyAtmThickness);
                                if (ImGui::SliderFloat("Planet Scale", &planetScale, 5000.0f, 15000.0f, "%.2f"))
                                    lighting->setPlanetScale(planetScale);
                                ImGui::TextDisabled("Lower density/thickness to feel higher altitude.");
                                ImGui::EndTabItem();
                            }

                            if (ImGui::BeginTabItem("Clouds"))
                            {
                                if (ImGui::Checkbox("Clouds Enabled", &cloudsEnabled))
                                lighting->setCloudsEnabled(cloudsEnabled);
                                if (ImGui::SliderFloat("Cloud Density", &cloudDensity, 0.02f, 0.2f, "%.3f"))
                                    lighting->setCloudDensity(cloudDensity);
                                if (ImGui::SliderFloat("Cloud Sigma T", &cloudSigmaT, 1.0f, 10.0f, "%.1f"))
                                    lighting->setCloudSigmaT(cloudSigmaT);
                                if (ImGui::ColorEdit3("Cloud Albedo", &cloudAlbedo.x))
                                    lighting->setCloudAlbedo(cloudAlbedo);
                                if (ImGui::SliderFloat("Cloud Step Count", &cloudStepCount, 32.0f, 96.0f, "%.1f"))
                                    lighting->setCloudStepCount(cloudStepCount);
                                    
                                if (ImGui::SliderFloat("Cloud Sigma S", &cloudSigmaS, 1.0f, 10.0f, "%.1f"))
                                    lighting->setCloudSigmaS(cloudSigmaS);
                                if (ImGui::SliderFloat("Cloud Phase G", &cloudPhaseG, 0.0f, 1.0f, "%.1f"))
                                    lighting->setCloudPhaseG(cloudPhaseG);

                                ImGui::Separator();

                                ImGui::Text("Cloud Shape & Movement");
                                float cloudEdgeFeather = lighting->getCloudEdgeFeather();
                                float cloudNoiseScale = lighting->getCloudNoiseScale();
                                float cloudNoiseContrastLo = lighting->getCloudNoiseContrastLo();
                                float cloudNoiseContrastHi = lighting->getCloudNoiseContrastHi();
                                float cloudWindSpeed = lighting->getCloudWindSpeed();
                                glm::vec2 cloudWindDir = lighting->getCloudWindDir();

                                if (ImGui::SliderFloat("Cloud Edge Feather", &cloudEdgeFeather, 1.0f, 30.0f, "%.1f"))
                                    lighting->setCloudEdgeFeather(cloudEdgeFeather);
                                if (ImGui::SliderFloat("Cloud Noise Scale", &cloudNoiseScale, 0.005f, 0.05f, "%.3f"))
                                    lighting->setCloudNoiseScale(cloudNoiseScale);
                                if (ImGui::SliderFloat("Cloud Contrast Lo", &cloudNoiseContrastLo, 0.0f, 1.0f, "%.2f"))
                                    lighting->setCloudNoiseContrastLo(cloudNoiseContrastLo);
                                if (ImGui::SliderFloat("Cloud Contrast Hi", &cloudNoiseContrastHi, 0.0f, 1.0f, "%.2f"))
                                    lighting->setCloudNoiseContrastHi(cloudNoiseContrastHi);
                                if (ImGui::SliderFloat("Cloud Wind Speed", &cloudWindSpeed, 0.0f, 100.0f, "%.1f"))
                                    lighting->setCloudWindSpeed(cloudWindSpeed);
                                if (ImGui::SliderFloat2("Cloud Wind Direction", &cloudWindDir.x, -1.0f, 1.0f, "%.2f"))
                                    lighting->setCloudWindDir(cloudWindDir);
                                ImGui::EndTabItem();
                            }
                        }
                        ImGui::EndTabBar();
                    	
                    }

                	ImGui::Separator();
                	if (ImGui::CollapsingHeader("Water")) {
                		ImGui::SliderFloat("Water wave strength", &waterRenderer->waveStrength, 0.000f, 0.09f, "%.3f");
                		ImGui::SliderFloat("Water dudv tiling", &waterRenderer->dudvTiling, 0.000f, 0.09f, "%.2f");

                	}

					ImGui::Separator();
					if (ImGui::CollapsingHeader("Network Debug")) {
                        auto netStats = camera->getReconcileDebugStats();
                        ImGui::Text("Client Tick: %d", clientTick);
                        ImGui::Text("Last Ack Tick: %d", camera->getLastAppliedAckTick());
                        ImGui::Text("Pending Snapshot Tick: %d", camera->getPendingCorrectionTick());
                     ImGui::Text("Last effective ack tick: %d", netStats.lastEffectiveAckTick);
                        ImGui::Text("Predicted States: %zu", camera->getPredictedStateCount());
                        ImGui::Text("Pending Inputs: %zu", camera->getPendingInputCount());
                        ImGui::Text("Corrections total/applied/ignored: %llu / %llu / %llu",
                            static_cast<unsigned long long>(netStats.totalCorrections),
                            static_cast<unsigned long long>(netStats.appliedCorrections),
                            static_cast<unsigned long long>(netStats.ignoredCorrections));
                        ImGui::Text("Suspected 1-tick phase mismatch count: %llu",
                            static_cast<unsigned long long>(netStats.suspectedOffByOneCorrections));
                        ImGui::Text("Last errors: pos=%.6f vel=%.6f horiz=%.6f vert=%.6f",
                            netStats.lastPosErr,
                            netStats.lastVelErr,
                            netStats.lastHorizontalErr,
                            netStats.lastVerticalErr);
                        ImGui::Text("Ack match check: err(ack)=%.6f err(ack-1)=%.6f",
                            netStats.lastErrAtAckTick,
                            netStats.lastErrAtAckMinusOneTick);

						if (uiInteractive) {
							float simLatMs = udpClient->getSimulatedLatency();
							if (ImGui::SliderFloat("Sim Latency (ms)", &simLatMs, 0.0f, 500.0f, "%.0f ms"))
								udpClient->setSimulatedLatency(simLatMs);

                            float posThreshold = camera->getReconcilePosErrorThreshold();
                            if (ImGui::SliderFloat("Reconcile Pos Threshold", &posThreshold, 0.01f, 0.5f, "%.3f"))
                                camera->setReconcilePosErrorThreshold(posThreshold);

                            float velThreshold = camera->getReconcileVelErrorThreshold();
                            if (ImGui::SliderFloat("Reconcile Vel Threshold", &velThreshold, 0.001f, 0.5f, "%.3f"))
                                camera->setReconcileVelErrorThreshold(velThreshold);

                            bool reconcileLogEnabled = camera->isReconcileLogEnabled();
                            if (ImGui::Checkbox("Verbose Reconcile Logs", &reconcileLogEnabled))
                                camera->setReconcileLogEnabled(reconcileLogEnabled);

                            bool reconcileAutoPhaseAdjust = camera->isReconcileAutoPhaseAdjustEnabled();
                            if (ImGui::Checkbox("Auto Ack Phase Adjust", &reconcileAutoPhaseAdjust))
                                camera->setReconcileAutoPhaseAdjustEnabled(reconcileAutoPhaseAdjust);

							ImGui::TextDisabled("Simulates S->C receive delay for reconciliation testing.");
						} else {
							ImGui::Text("Sim Latency: %.0f ms", udpClient->getSimulatedLatency());
						}
					}

                    ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Settings")) {
	                if (ImGui::DragFloat("Dbg window Font Size", &style.FontSizeBase, 0.20f, 5.0f, 100.0f, "%.0f"))
	                	style._NextFrameFontSizeBase = style.FontSizeBase;

                	ImGui::EndTabItem();
                }
                if (ImGui::BeginTabItem("Profiler")) {
                    ImGui::Checkbox("Open Profiler Window", &showProfilerWindow);
                    profilingEnabled = showProfilerWindow;
                    ImGui::EndTabItem();
                } else if (!showProfilerWindow) {
                    profilingEnabled = false;
                }
                ImGui::EndTabBar();
            }

			static bool spectator = false;
			ImGui::Separator();
			if (ImGui::Checkbox("Survival", &spectator))
			{
				if (spectator)
				{
					NetMessage pkt;
					pkt.message = "/gamemode survival";
					udpClient->sendPacket(pkt);
				}
				else
				{
					NetMessage pkt;
					pkt.message = "/gamemode spectator";
					udpClient->sendPacket(pkt);
				}
			}


            ImGui::End();
            if (!uiInteractive) {
                ImGui::PopStyleVar();
            }
        }

        // ── Detachable Profiler Window ──
        if (showProfilerWindow) {
            ImGui::SetNextWindowSize(ImVec2(580, 400), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowPos(ImVec2(600, 10), ImGuiCond_FirstUseEver);
            ImGuiWindowFlags profFlags = 0;
            if (!uiInteractive) {
                profFlags |= ImGuiWindowFlags_NoInputs;
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
            }
            if (ImGui::Begin("GPU Profiler", &showProfilerWindow, profFlags)) {
                ImGui::Text("FPS: %.1f (%.3f ms/frame)", uiDisplayFPS, uiDisplayFPS > 0.0f ? 1000.0f / uiDisplayFPS : 0.0f);
                ImGui::Separator();

                // Calculate totals
                float totalGPU = static_cast<float>(
                    measuredAverageMsDrawSky + measuredAverageMsDrawClouds + measuredAverageMsRenderShader +
                    measuredAverageMsDrawShadows + measuredAverageMsDrawWaterReflection +
                    measuredAverageMsRenderWater + measuredAverageMsDrawEntities);

                // Frame budget target
                static float targetFPS = 60.0f;
                float targetFrameTime = 1000.0f / targetFPS;

                auto showTimingBar = [&](const char* label, double ms, ImVec4 color) {
                    float msf = static_cast<float>(ms);
                    float percent = totalGPU > 0.0f ? (msf / totalGPU) * 100.0f : 0.0f;

                    ImGui::Text("%-20s", label);
                    ImGui::SameLine();

                    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, color);
                    ImGui::ProgressBar(msf / targetFrameTime, ImVec2(200, 0), "");
                    ImGui::PopStyleColor();

                    ImGui::SameLine();
                    ImGui::Text("%.3f ms (%.1f%%)", msf, percent);
                };

                showTimingBar("Sky",           measuredAverageMsDrawSky,             ImVec4(0.2f, 0.6f, 1.0f, 1.0f));
                showTimingBar("Clouds",        measuredAverageMsDrawClouds,          ImVec4(0.8f, 0.8f, 0.9f, 1.0f));
                showTimingBar("Terrain",       measuredAverageMsRenderShader,        ImVec4(0.4f, 0.8f, 0.4f, 1.0f));
                showTimingBar("Shadows",       measuredAverageMsDrawShadows,         ImVec4(0.3f, 0.3f, 0.3f, 1.0f));
                showTimingBar("Water Reflect", measuredAverageMsDrawWaterReflection, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
                showTimingBar("Water Render",  measuredAverageMsRenderWater,         ImVec4(0.1f, 0.4f, 0.8f, 1.0f));
                showTimingBar("Entities",      measuredAverageMsDrawEntities,        ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
                showTimingBar("SSAO",          measuredAverageMsSSAO,                ImVec4(0.6f, 0.2f, 0.8f, 1.0f));
                ImGui::Separator();
                ImGui::Text("Total GPU: %.3f ms (%.1f FPS budget)", totalGPU, totalGPU > 0.0f ? 1000.0f / totalGPU : 0.0f);

                // Color-coded frame budget indicator
                if (totalGPU > targetFrameTime) {
                    ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.3f, 1.0f), "WARNING: Over frame budget!");
                } else if (totalGPU > targetFrameTime * 0.8f) {
                    ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "CAUTION: Near frame budget (>80%%)");
                } else {
                    ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.3f, 1.0f), "Performance OK");
                }

                // ── Settings ──
                ImGui::Separator();
                if (ImGui::CollapsingHeader("Profiler Settings")) {
                    ImGui::SliderFloat("Smoothing (EMA alpha)", &profilingEMASmoothing, 0.01f, 0.5f, "%.2f");
                    ImGui::SameLine();
                    if (ImGui::SmallButton("?")) {
                        ImGui::SetTooltip("Lower = smoother/slower, Higher = noisier/faster response");
                    }
                    ImGui::SliderFloat("Target FPS", &targetFPS, 30.0f, 240.0f, "%.0f");

                    if (ImGui::Button("Reset Averages")) {
                        measuredAverageMsDrawSky = 0.0;
                        measuredAverageMsDrawClouds = 0.0;
                        measuredAverageMsDrawWaterReflection = 0.0;
                        measuredAverageMsDrawShadows = 0.0;
                        measuredAverageMsRenderShader = 0.0;
                        measuredAverageMsRenderWater = 0.0;
                        measuredAverageMsDrawEntities = 0.0;
                        measuredAverageMsSSAO = 0.0;
                    }
                }
            }
            ImGui::End();
            if (!uiInteractive) {
                ImGui::PopStyleVar();
            }

            // Keep profiling active while window is open
            profilingEnabled = showProfilerWindow;
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

    // Query objects (profiling)
    glDeleteQueries(QUERY_POOL_SIZE, queryDrawEntities);
    glDeleteQueries(QUERY_POOL_SIZE, queryDrawSkyPool);
    glDeleteQueries(QUERY_POOL_SIZE, queryDrawCloudsPool);
    glDeleteQueries(QUERY_POOL_SIZE, queryDrawWaterReflectionPool);
    glDeleteQueries(QUERY_POOL_SIZE, queryRenderWaterPool);
    glDeleteQueries(QUERY_POOL_SIZE, queryRenderShaderPool);
    glDeleteQueries(QUERY_POOL_SIZE, queryDrawShadowsPool);

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
	controlsArray[THIRD_PERSON_CAMERA]	= GLFW_KEY_F5;
	
	controlsArray[HOTBAR_1]				= GLFW_KEY_1;
	controlsArray[HOTBAR_2]				= GLFW_KEY_2;
	controlsArray[HOTBAR_3]				= GLFW_KEY_3;
	controlsArray[HOTBAR_4]				= GLFW_KEY_4;
	controlsArray[HOTBAR_5]				= GLFW_KEY_5;
	controlsArray[HOTBAR_6]				= GLFW_KEY_6;
	controlsArray[HOTBAR_7]				= GLFW_KEY_7;
	controlsArray[HOTBAR_8]				= GLFW_KEY_8;
	controlsArray[HOTBAR_9]				= GLFW_KEY_9;
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
	
	if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
        keys |= IN_DROP;

	uint8_t activeHotbarSlot = -1;
	if (glfwGetKey(window, controlsArray[HOTBAR_1]) == GLFW_PRESS) activeHotbarSlot = 0;
	if (glfwGetKey(window, controlsArray[HOTBAR_2]) == GLFW_PRESS) activeHotbarSlot = 1;
	if (glfwGetKey(window, controlsArray[HOTBAR_3]) == GLFW_PRESS) activeHotbarSlot = 2;
	if (glfwGetKey(window, controlsArray[HOTBAR_4]) == GLFW_PRESS) activeHotbarSlot = 3;
	if (glfwGetKey(window, controlsArray[HOTBAR_5]) == GLFW_PRESS) activeHotbarSlot = 4;
	if (glfwGetKey(window, controlsArray[HOTBAR_6]) == GLFW_PRESS) activeHotbarSlot = 5;
	if (glfwGetKey(window, controlsArray[HOTBAR_7]) == GLFW_PRESS) activeHotbarSlot = 6;
	if (glfwGetKey(window, controlsArray[HOTBAR_8]) == GLFW_PRESS) activeHotbarSlot = 7;
	if (glfwGetKey(window, controlsArray[HOTBAR_9]) == GLFW_PRESS) activeHotbarSlot = 8;

	if (activeHotbarSlot != (uint8_t)-1) inventoryUI->activeHotbarSlot = activeHotbarSlot;

	inputs.keys = keys;
	inputs.pitch = camera->getPlayer()->getPitch();
	inputs.yaw = camera->getPlayer()->getYaw();
	inputs.loadRadius = camera->getPlayer()->getLoadRadius();
	inputs.activeHotbarSlot = activeHotbarSlot;
	inputs.serverClientReconciliationTick = clientTick;

	return inputs;
}

void App::processInputMenus(int key, int action) {

	auto manager = menuManager.lock();

	// HANDLE EVENTS WHEN CHAT OPEN

	if (manager && key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		menuManager.reset();
		if (!uiInteractive)
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}

	//TODO : Change gamemode for player on chat too so prediction works on other modes other than spectator when changing gamemode by chat.
	if (manager == chat)
	{
		if (key == GLFW_KEY_ENTER && action == GLFW_PRESS)
		{
			if (chat->currMsg.empty()) return ; //will this return be safe in the future?
			NetMessage pkt;
			pkt.message = chat->currMsg;
			udpClient->sendPacket(pkt);

			chat->cleanMsgSent();
		}
		if (key == GLFW_KEY_BACKSPACE && (action == GLFW_PRESS || action == GLFW_REPEAT))
			chat->removeCharFromCurrMsg();
		if ((key == GLFW_KEY_UP || key == GLFW_KEY_DOWN) && (action == GLFW_PRESS || action == GLFW_REPEAT))
			chat->goThroughChatLog(key);
	}

	// CHOSE MENU (order here IS important. must do after handling events)
	if (!manager)
	{
		if (key == GLFW_KEY_ENTER && action == GLFW_PRESS)
			menuManager = chat;
		if (key == GLFW_KEY_E && action == GLFW_PRESS)
		{
			menuManager = inventoryUI;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
	}
}


// TODO : put actions in corresponding functions for clarity
void App::processInput() {
    static bool f11Held = false;
    static bool f1Held  = false;
    static bool f2Held  = false;
    static bool f4Held  = false;
	static bool ThirdPersonCameraKeyActive = false;
    static bool tabHeld = false;

	//reload chunk. F3 + A; TODO : also add the neighbours logic. Otherwise some "walls" could be rendered
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

	if (glfwGetKey(window, controlsArray[THIRD_PERSON_CAMERA]) == GLFW_PRESS && !ThirdPersonCameraKeyActive) {
		ThirdPersonCameraKeyActive = true;
		camera->toggleThirdPersonCamera();
	}
	if (glfwGetKey(window, controlsArray[THIRD_PERSON_CAMERA]) == GLFW_RELEASE && ThirdPersonCameraKeyActive) {
		ThirdPersonCameraKeyActive = false;
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
    // if (glfwGetKey(window, controlsArray[CLOSE_WINDOW]) == GLFW_PRESS)
    //     glfwSetWindowShouldClose(window, true);
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
#elif defined(_WIN32)
    PROCESS_MEMORY_COUNTERS pmc;
    if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
        return pmc.WorkingSetSize;
    }
    return 0;
#else
    return 0;
#endif
}
