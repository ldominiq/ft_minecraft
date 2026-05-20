//
// Created by lucas on 6/25/25.
//

#include "App.hpp"

App::App(const std::string& serverIp):
			camera(nullptr),
			monitor(nullptr),
			mode(nullptr),
			serverIp(serverIp),

            lighting(nullptr),
            textureShader(nullptr),
            activeShader(nullptr) {

    // Pre-allocate the FPS sample buffer to avoid reallocations at runtime
    fpsSamples.reserve(fpsSampleCount);
}

App::~App() { cleanup(); }

GLFWimage load_icon(const char* path) {
    GLFWimage image;
    int channels;
    image.pixels = stbi_load(path, &image.width, &image.height, &channels, 4);
    if (!image.pixels) {
        std::cerr << "Failed to load window icon: " << stbi_failure_reason() << std::endl;
    }
    return image;
}

void App::init(const std::string& serverIp) {
    std::string targetIp = serverIp;

    {
        // Check for a server.txt file in the current directory
        std::ifstream serverFile("server.txt");
        if (serverFile.is_open()) {
            std::string line;
            if (std::getline(serverFile, line) && !line.empty()) {
                targetIp = line;
                std::cout << "[Config] Found server.txt, overriding IP with: " << targetIp << std::endl;
            }
        }
    }

    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
	glfwWindowHint(GLFW_SAMPLES, 8); // 8x MSAA

    // Monitor infos
    monitor = glfwGetPrimaryMonitor();
    mode = glfwGetVideoMode(monitor);

	std::cout << "[Config] Using monitor resolution: " << mode->width << "x" << mode->height << std::endl;

    window = glfwCreateWindow(windowedWidth, windowedHeight, "ft_minecraft", nullptr, nullptr);
    glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
	glfwSetWindowUserPointer(window, this);

    // Set the window icon
    GLFWimage image;
    image = load_icon("assets/textures/icon.png");
    if (image.pixels) {
        glfwSetWindowIcon(window, 1, &image);
        stbi_image_free(image.pixels); // <- free stb allocation
        image.pixels = nullptr;
    }
    
    audio = std::make_unique<AudioManager>();
    // If SoLoud can't open a backend (headless / no audio device / driver mismatch),
    // drop the manager rather than leave it half-initialised — every playSfx*/update
    // call later guards on `if (audio)`, so the game runs silent instead of crashing.
    if (!audio->init()) {
        std::cerr << "[Audio] disabled (init failed)" << std::endl;
        audio.reset();
    }

    glfwSetFramebufferSizeCallback(window, [](GLFWwindow* w, const int width, const int height) {
		App* app = static_cast<App*>(glfwGetWindowUserPointer(w));
		// Skip resize if window is minimized (0x0)
		if (width == 0 || height == 0)
			return;
        glViewport(0, 0, width, height);
		glfwGetFramebufferSize(w, &app->screenWidth, &app->screenHeight);
		auto manager = app->menuManager.lock();
		if (manager)
			manager->resize(width, height);
		if (app->inventoryUI && manager != app->inventoryUI)
			app->inventoryUI->resize(width, height);
		if (app->chat && manager != app->chat)
			app->chat->resize(width, height);
		if (app->debugHUD) app->debugHUD->resize(width, height);
		if (app->playerListHUD) app->playerListHUD->resize(width, height);
		if (app->mainMenu) app->mainMenu->resize(width, height);
		if (app->multiplayerMenu) app->multiplayerMenu->resize(width, height);
		if (app->settingsMenu) app->settingsMenu->resize(width, height);
		if (app->pauseMenu) app->pauseMenu->resize(width, height);
		if (app->controlsMenu) app->controlsMenu->resize(width, height);
    });

    glfwMakeContextCurrent(window);
    gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress));

    glfwGetFramebufferSize(window, &screenWidth, &screenHeight);
    glViewport(0, 0, screenWidth, screenHeight);

	renderer = std::make_unique<Renderer>();

	// ********************Water Renderer setup******************************
	// Match the scene FBO's HDR state so the reflection/refraction targets
	// don't clamp linear-HDR radiance to [0,1] before water.frag samples them.
	waterRenderer = std::make_unique<WaterRenderer>(screenWidth, screenHeight);

	// ********************Chunk Boundary Renderer**************************
	chunkBoundaryRenderer = std::make_unique<ChunkBoundaryRenderer>();

	// ********************Render Type Debug Framebuffers********************
	renderTypeFramebuffer = std::make_unique<RenderTypeFramebuffer>(screenWidth, screenHeight);

	loader = std::make_unique<Loader>();
	// GUI textures are now dynamically managed based on debug flags
    guiRenderer = std::make_unique<GuiRenderer>(*loader);

    lighting = std::make_unique<Lighting>(screenWidth, screenHeight);
    lighting->setHDREnabled(hdrEnabled);
    lighting->setSkyExposure(manualExposure);

	chat = std::make_shared<Chat>(screenWidth, screenHeight);
	debugHUD = std::make_unique<DebugHUD>(screenWidth, screenHeight);
	playerListHUD = std::make_unique<PlayerListHUD>(screenWidth, screenHeight);

	m_itemPropEntityManager = std::make_unique<ItemPropEntityManager>(&textureManager);

    // Initialize terrain debug window for parameter tweaking
    terrainDebugWindow = std::make_unique<TerrainDebugWindow>();
    // Set callback to send terrain params to server when changed
    terrainDebugWindow->setSendParamsCallback([this](const NetTerrainParams& pkt) {
        if (udpClient) {
            NetTerrainParams copy = pkt;
            udpClient->sendPacket(copy);
        }
    });

    gBuffer = std::make_shared<GBuffer>(screenWidth, screenHeight);
    ssao = std::make_shared<SSAO>(screenWidth, screenHeight);

    // Scene FBO (MSAA) — sample count must match the GLFW window hint above.
    // hdrEnabled selects between GL_RGBA16F (HDR) and GL_RGBA8 (LDR fallback).
    sceneFBO = std::make_unique<SceneFramebuffer>(screenWidth, screenHeight, 8, hdrEnabled);

    // Auto-exposure: PBO-based luminance readback. Cheap (<0.1ms), 1-frame latency.
    autoExposure = std::make_unique<AutoExposure>();

    glEnable(GL_DEPTH_TEST);
    
    // enable face culling
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    glEnable(GL_MULTISAMPLE);


    lighting->initCSMResources();


    // Mouse movement event handling
    camera = std::make_unique<Camera>(glm::vec3(0.0f, 128.0f, 0.0f));

	std::shared_ptr<PlayerInventory> inv = camera->getPlayer()->inventory;
	std::shared_ptr<CraftingStation> craft = camera->getPlayer()->craftingStation;
	inventoryUI = std::make_shared<InventoryUI>(windowedWidth, windowedHeight, &textureManager, inv, craft, camera->getPlayer()->inventoryExternalVarsRefs);
	inventoryUI->resize(screenWidth, screenHeight);

    glfwSetCursorPosCallback(window, [](GLFWwindow* w, const double xpos, const double ypos) {
        static App* app = static_cast<App*>(glfwGetWindowUserPointer(w));
        if (!app) return;

		// In non-Playing states, forward to menu and skip camera
		if (app->gameState != GameState::Playing) {
			auto menuManagerPtr = app->menuManager.lock();
			if (menuManagerPtr)
				menuManagerPtr->handleMouseMove(xpos, ypos);
			return;
		}

        // Honour ImGui’s mouse capture: if the UI is being interacted with
        // (e.g. hovering/clicking in a window), do not rotate the camera.
        ImGuiIO& io = ImGui::GetIO();

		//scale back down values to counteract wayland bugs
		// float xscale, yscale;
		// glfwGetWindowContentScale(w, &xscale, &yscale);

		double mouseX, mouseY;
		mouseX = xpos;
		mouseY = ypos;
		// mouseX = xpos * xscale;
		// mouseY = ypos * yscale;

		auto menuManagerPtr = app->menuManager.lock();
		if (menuManagerPtr)
		{
			menuManagerPtr->handleMouseMove(mouseX, mouseY);
            app->lastX = mouseX;
            app->lastY = mouseY;
			return ;
		}

        if (io.WantCaptureMouse || app->uiInteractive) {
            return;
        }
        if (app->firstMouse) {
            app->lastX = mouseX;
            app->lastY = mouseY;
            app->firstMouse = false;
        }
        const float xoffset = static_cast<float>(mouseX - app->lastX);
        const float yoffset = static_cast<float>(app->lastY - mouseY); // Reversed: y-coordinates go from bottom to top
        app->lastX = mouseX;
        app->lastY = mouseY;

        if (!app->camera || !app->camera->getPlayer() || app->camera->getPlayer()->health <= 0) return; // don't allow clicking if player is dead or camera not initialized

        app->camera->processMouseMovement(xoffset, yoffset);

		app->mouseMovedRecently = true;
		app->lastMouseMoveTime = glfwGetTime();
    });

	glfwSetCharCallback(window, [](GLFWwindow* w, unsigned int codepoint) {
		App* app = static_cast<App*>(glfwGetWindowUserPointer(w));
		if (!app) return;

		if (app->gameState == GameState::Multiplayer) {
			app->multiplayerMenu->addChar(static_cast<char>(codepoint));
			return;
		}
		else if (app->gameState == GameState::Settings) {
			app->settingsMenu->addChar(static_cast<char>(codepoint));
			return;
		}

		auto manager = app->menuManager.lock();
		if (manager != app->chat) return ;

		app->chat->addCharToCurrMsg(static_cast<char>(codepoint));
	});

	glfwSetKeyCallback(window, [](GLFWwindow* w, int key, int scancode, int action, int mods) {
		App* app = static_cast<App*>(glfwGetWindowUserPointer(w));
		if (!app) return;

		// In non-Playing states, handle ESC to go back / don't close window
		if (app->gameState != GameState::Playing) {

			if (key == app->controlsArray[TOGGLE_FULLSCREEN] && action == GLFW_PRESS && !(app->gameState == GameState::Controls && app->controlsMenu->getChangeRequested()))
				app->toggleDisplayMode();
			app->processInputMenus(key, action);
			return;
		}

		auto manager = app->menuManager.lock();

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
			
			//scale back down values to counteract wayland bugs
			// float xscale, yscale;
			// glfwGetWindowContentScale(w, &xscale, &yscale);

			// mouseX = mouseX * xscale;
			// mouseY = mouseY * yscale;

			if (app->gameState != GameState::Playing) {
				manager->handleMouseClick(mouseX, mouseY, button, action);

				return;
			}

			if (manager == app->pauseMenu) {
				manager->handleMouseClick(mouseX, mouseY, button, action);
				return;
			}
			if (manager == app->inventoryUI)
			{
				manager->handleMouseClick(mouseX, mouseY, button, action);
				if (app->inventoryUI->lastAction.has_value() && app->udpClient)
				{
					auto& pkt = *app->inventoryUI->lastAction;
					app->udpClient->sendPacket(pkt);
					app->inventoryUI->lastAction.reset();
				}
			}
			return ;
		}

		if (app->gameState != GameState::Playing || !app->udpClient) return;

		//kinda weird way to do it.
		uint8_t mouseButtons = 0;
		if (action == GLFW_PRESS) {
			if (button == app->controlsArray[DESTROY_BLOCK]) {
				mouseButtons |= IN_LEFT_CLICK;
			} else if (button == app->controlsArray[PLACE_BLOCK]) {
				mouseButtons |= IN_RIGHT_CLICK;
			}
		}

		if (!app->camera || !app->camera->getPlayer() || app->camera->getPlayer()->health <= 0) return; // don't allow clicking if player is dead or camera not initialized

		if (mouseButtons && app->camera && app->camera->getPlayer())
			app->camera->getPlayer()->triggerArmSwing();

		// Self-feedback on left-click: play the attack swing ONLY when the click would actually
		// hit a mob/player. Server resolves the hit via the same getTarget raycast at attack-tick
		// time, so client and server agree (modulo ~1 tick of network desync, acceptable for sfx).
		// Empty swings stay silent; vanilla does the same. The victim's hurt sound (bit 0x10) is
		// what tells the player "you connected" and arrives from the server moments later.
		if ((mouseButtons & IN_LEFT_CLICK) && app->audio && app->renderer && app->camera) {
			auto local = app->camera->getPlayer();
			if (local) {
				glm::ivec3 hitBlock{}, faceNormal{};
				LivingEntity* victim = nullptr;
				if (app->renderer->getTarget(*local, hitBlock, faceNormal, victim) == TargetType::LivingEntity
				    && victim != nullptr
				    && victim != local.get()) {
					app->audio->playSfx2D(SoundId::Player_AttackSwing, 0.7f);
				}
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

	renderer->livingEntitiesManager.add(camera->getPlayer());

    // Generate query pools
    glGenQueries(QUERY_POOL_SIZE, queryDrawSkyPool);
    glGenQueries(QUERY_POOL_SIZE, queryDrawCloudsPool);
    glGenQueries(QUERY_POOL_SIZE, queryDrawWaterReflectionPool);
    glGenQueries(QUERY_POOL_SIZE, queryDrawWaterRefractionPool);
    glGenQueries(QUERY_POOL_SIZE, queryGBufferPool);
    glGenQueries(QUERY_POOL_SIZE, queryDrawShadowsPool);
    glGenQueries(QUERY_POOL_SIZE, queryRenderShaderPool);
    glGenQueries(QUERY_POOL_SIZE, queryRenderWaterPool);
    glGenQueries(QUERY_POOL_SIZE, queryDrawEntities);
    glGenQueries(QUERY_POOL_SIZE, querySSAOPool);

	// Create main menu screens
	menuDirtTex = Menu::loadTexture2D("assets/textures/block/dirt.png");
	mainMenu = std::make_shared<MainMenu>(screenWidth, screenHeight, menuDirtTex);
	multiplayerMenu = std::make_shared<MultiplayerMenu>(screenWidth, screenHeight, menuDirtTex);
	settingsMenu = std::make_shared<SettingsMenu>(screenWidth, screenHeight, menuDirtTex);
	pauseMenu = std::make_shared<PauseMenu>(screenWidth, screenHeight);
	controlsMenu = std::make_shared<ControlsMenu>(screenWidth, screenHeight, menuDirtTex);
	controlsArray = controlsMenu->getControlsArray();

	mainMenu->setButtonCallback([this](int btn) {
		switch (btn) {
			case 0: break; // Singleplayer - disabled
			case 1: transitionTo(GameState::Multiplayer); break;
			case 2: transitionTo(GameState::Settings); break;
			case 3: glfwSetWindowShouldClose(window, true); break;
		}
	});

	multiplayerMenu->setConnectCallback([this](const std::string& ip) {
		if (ip.empty()) {
			multiplayerMenu->setErrorMessage("Please enter a server address.");
			return;
		}
		if (!connectToServer(ip))
			return;
		multiplayerMenu->setErrorMessage("Connecting...");
		connectPending = true;
		connectStartTime = static_cast<float>(glfwGetTime());
	});
	multiplayerMenu->setCancelCallback([this]() {
		if (connectPending) {
			connectPending = false;
			udpClient.reset();
			clientConnected = false;
		}
		transitionTo(GameState::MainMenu);
	});

	settingsMenu->setDoneCallback([this]() {
		transitionTo(GameState::MainMenu);
	});
	settingsMenu->setChangeControlsCallback([this]() {
		transitionTo(GameState::Controls);
	});

	pauseMenu->setContinueCallback([this]() {
		menuManager.reset();
		if (!uiInteractive)
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	});
	pauseMenu->setBackToMainMenuCallback([this]() {
		if (udpClient && clientConnected) {
			NetDisconnect pkt;
			pkt.username = settingsMenu ? settingsMenu->getUsername() : "";
			udpClient->sendPacket(pkt);
		}
		connectPending = false;
		udpClient.reset();
		clientConnected = false;
		renderer->clearCache();

		transitionTo(GameState::MainMenu);
	});

	controlsMenu->setSaveCallback([this]() {
		transitionTo(GameState::Settings);
	});

	transitionTo(GameState::MainMenu);
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
				localClientId     = p.clientId;
				localPlayerListId = p.playerListId;
				break;
			}

			case PacketType::NET_SET_NAME: {
				auto& p = static_cast<NetSetName&>(*pkt);
				camera->getPlayer()->setName(p.username);
				chat->updateChatlog("Your name has been changed to " + p.username);
				settingsMenu->setUsername(p.username);
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
				camera->onSnapshot(p);
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
				// Explosion death (type==-1, bit 0x20): fire immediately so the boom is in
				// sync with the visual blast and so the suppression window is pushed before
				// the same burst's MODIFIED_BLOCK_DATA packets play their crater sounds.
				if (audio
				    && p.eEntityType == EEntityTypes::LIVING_ENTITIES
				    && p.type == static_cast<uint16_t>(-1)
				    && (p.positionFlags & 0x20)) {
					glm::dvec3 epos(p.positionX, p.positionY, p.positionZ);
					glm::dvec3 listenerPos = camera ? camera->getEyePosD() : epos;
					const void* key = nullptr;
					for (const auto& le : renderer->livingEntities) {
						if (le && le->getID() == p.entityID) { key = le.get(); break; }
					}
					audio->onCreeperExploded(key, epos, listenerPos);
				}
				// Hurt one-shot (server bit 0x10). Skip on the death packet (type == -1) — the
				// AudioManager's death-edge sweep handles that case with the proper death sfx.
				if (audio
				    && p.eEntityType == EEntityTypes::LIVING_ENTITIES
				    && p.type != static_cast<uint16_t>(-1)
				    && (p.positionFlags & 0x10)) {
					glm::dvec3 epos(p.positionX, p.positionY, p.positionZ);
					audio->playSfx3D(
						AudioManager::hurtSoundFor(static_cast<LivingEntityType>(p.type)),
						epos, glm::vec3(0.0f), 1.0f);
				}
				// Item pickup: server tags the entity-removal packet for an ITEMS entity by
				// setting type == -1 and rewriting the position to the picker's location
				// (see World::pickupItem). Play the generic block-pop one-shot there so both
				// the local player and nearby remote players get audible feedback. 3D so it
				// attenuates if it was someone else picking up an item across the map.
				if (audio
				    && p.eEntityType == EEntityTypes::ITEMS
				    && p.type == static_cast<uint16_t>(-1)) {
					glm::dvec3 epos(p.positionX, p.positionY, p.positionZ);
					audio->playSfx3D(SoundId::Block_Pop, epos, glm::vec3(0.0f), 0.6f);
				}
				break;
			}

			case PacketType::NET_INVENTORY: {
				auto& p = static_cast<NetInventory&>(*pkt);
				if (static_cast<InventoryType>(p.inventoryTypeID) == InventoryType::PLAYER)
					camera->getPlayer()->inventory->setSlot(p.slot, p.amount, p.type);
				else if (static_cast<InventoryType>(p.inventoryTypeID) == InventoryType::CRAFTING_STATION)
					camera->getPlayer()->craftingStation->setSlot(p.slot, p.amount, p.type);

				break;
			}

			case PacketType::MODIFIED_BLOCK_DATA: {
				auto& p = static_cast<NetModifiedBlockData&>(*pkt);
				// Look up the OLD block before applying the chunk update so we can pick the
				// right break/place sound (break = use the old block's material).
				BlockType oldBlock = renderer->getBlockWorld({p.x, p.y, p.z});
				BlockType newBlock = static_cast<BlockType>(p.blockType);
				renderer->updateChunk(p);

				if (audio) {
					// Stay inside kAttenMax (24m). LINEAR_DISTANCE gives 0 past it, so SoLoud
					// would kill the voice mid-buffer with an audible click on every distant
					// block update (water spread, far players mining, etc).
					glm::dvec3 center(p.x + 0.5, p.y + 0.5, p.z + 0.5);
					constexpr double kBlockSfxMaxDist = 20.0;
					glm::dvec3 listener = camera ? camera->getEyePosD() : glm::dvec3(center);
					glm::dvec3 diff = center - listener;
					double d2 = glm::dot(diff, diff);
					if (d2 > kBlockSfxMaxDist * kBlockSfxMaxDist) {
						break;
					}

					if (audio->blockSfxSuppressed(center))
						break;
					// Same-burst defense: server's explodeAt() carves blocks before damaging
					// entities, so crater MODIFIED_BLOCK_DATA packets land before the death
					// packet that opens the suppression window. Primed creepers don't otherwise
					// break blocks, so this check is safe.
					{
						bool nearPrimed = false;
						constexpr double kPrimedSuppressR2 = 6.0 * 6.0;
						for (const auto& le : renderer->livingEntities) {
							if (!le) continue;
							auto cc = std::dynamic_pointer_cast<ClientCreeper>(le);
							if (!cc || !cc->clientPrimed) continue;
							glm::dvec3 dd = le->getPositionD() - center;
							if (glm::dot(dd, dd) <= kPrimedSuppressR2) { nearPrimed = true; break; }
						}
						if (nearPrimed) break;
					}

					// Liquid spread (water/lava) is a constant background of MODIFIED_BLOCK_DATA
					// packets; playing the default Stone break/place for them is the wrong sound
					// AND noisy. Skip the audio for liquid changes; everything else falls through.
					auto isLiquid = [](BlockType b) {
						return b == BlockType::WATER || b == BlockType::LAVA;
					};

					if (newBlock == BlockType::AIR && oldBlock != BlockType::AIR) {
						if (!isLiquid(oldBlock))
							audio->playSfx3D(AudioManager::breakFor(oldBlock), center);
					} else if (oldBlock == BlockType::AIR && newBlock != BlockType::AIR) {
						if (!isLiquid(newBlock))
							audio->playSfx3D(AudioManager::placeFor(newBlock), center);
					}
				}
				break;
			}

			case PacketType::NET_MESSAGE: {
				auto& p = static_cast<NetMessage&>(*pkt);
				chat->updateChatlog(p.message);
				break;
			}

            case PacketType::NET_IMGUI: {
                auto& p = static_cast<NetImGui&>(*pkt);
                // Push every biome packet to the audio manager
                // We can't gate on p.currentBiome != currentBiome here because
                // the very first packet may match the default 0 (PLAINS) and skip
                // starting the music entirely.
                if (audio) audio->setBiome(static_cast<BiomeType>(p.currentBiome));
                currentBiome = p.currentBiome;
                currentTerrainHeight = p.terrainHeight;
                currentSeaLevel = p.seaLevel;
                const int waterSurfaceY = p.seaLevel + 1;
                if (waterRenderer) waterRenderer->setSeaLevel(waterSurfaceY);
                ChunkRenderer::setSeaLevel(waterSurfaceY);
                if (lighting) lighting->setSeaLevel(static_cast<float>(waterSurfaceY));
                currentWorldSeed = p.worldSeed;
                currentContinentalness = p.continentalness;
                currentErosion = p.erosion;
                currentPeakValley = p.peakValley;
                currentTemperature = p.temperature;
                currentHumidity = p.humidity;
                currentContBucket    = p.contBucket;
                currentErosionBucket = p.erosionBucket;
                currentPVBucket      = p.pvBucket;
                currentTempBucket    = p.tempBucket;
                currentHumidBucket   = p.humidBucket;
                break;
            }

            case PacketType::NET_TERRAIN_PARAMS: {
                auto& p = static_cast<NetTerrainParams&>(*pkt);
                // Log receipt of terrain params update
                std::cout << "[Client] Received terrain parameters update from server (seed: " << p.seed << ")\n";
                // Could optionally cache these for UI display, but server will handle actual generation
                break;
            }

            case PacketType::NET_SKY_TIME: {
                auto& p = static_cast<NetSkyTime&>(*pkt);
                lighting->setSkyTimeOffset(p.skyTimeOffset);
                lighting->setSunYawDeg(p.sunYawDeg);
                lighting->setSkyTimePaused(p.skyTimePaused);
                lighting->setSunStepping(p.sunStepping);
                lighting->setSunPauseTimer(p.sunPauseTimer);
                lighting->setSunStepTimer(p.sunStepTimer);
                lighting->setSkyMode(p.skyMode);
                lighting->setSkyTimeSpeed(p.skyTimeSpeed);
                break;
            }

            case PacketType::NET_PONG: {
				auto& p = static_cast<NetPong&>(*pkt);
                if (p.timestamp != lastPingTimestamp) break; // discard stale pongs
                auto nowUs = static_cast<uint64_t>(
                    std::chrono::duration_cast<std::chrono::microseconds>(
						std::chrono::steady_clock::now().time_since_epoch()).count());
				float rttMs = static_cast<float>(nowUs - p.timestamp) / 1000.0f;
				pingMs = (pingMs < 0.0f) ? rttMs : pingMs + pingEMASmoothing * (rttMs - pingMs);
				// Report measured ping to server so it can broadcast to other players
				NetPlayerPing report;
				report.pingMs = pingMs;
				udpClient->sendPacket(report);
                break;
            }

			case PacketType::NET_PING_LIST: {
				auto& p = static_cast<NetPingList&>(*pkt);
                remotePings.clear();
				entityToPlayerListId.clear();
				for (const auto& e : p.entries) {
					remotePings[e.entityId]          = e.pingMs;
					entityToPlayerListId[e.entityId] = e.playerListId;
				}
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
    
    // Load individual block textures into a texture array. A false return
    // means the resource pack is missing/empty; TextureManager still builds a
    // checker-only atlas so we can keep launching (everything renders
    // magenta), but flag it loudly here so the cause is obvious.
    if (!textureManager.loadResourcePack("assets")) {
        std::cerr << "[App] Resource pack 'assets' missing or empty, "
                     "the world will render entirely as the missing-texture "
                     "checker. Restore assets/textures/block/ and "
                     "assets/textures/item/ to fix." << std::endl;
    }

    activeShader = textureShader;

    activeShader->use();
    activeShader->setInt("blockTextures", 0);

    // shader configuration
    // --------------------

	waterRenderer->setDependencies(lighting, renderer, camera);

    gBufferShader = std::make_shared<Shader>("shaders/ssao_geometry.vert", "shaders/ssao_geometry.frag");
    gBufferShader->use();
    gBufferShader->setInt("blockTextures", 0);

    // Z-prepass shader — minimal vertex transform + alpha-test discard.
    depthPrepassShader = std::make_shared<Shader>(
        "shaders/terrain_depth_prepass.vert", "shaders/terrain_depth_prepass.frag");
    depthPrepassShader->use();
    depthPrepassShader->setInt("blockTextures", 0);

    // Wire the texture manager and shaders to subsystems that need them
    renderer->setTextureManager(&textureManager);
    renderer->setVegetationShader(std::make_shared<Shader>("shaders/vegetation.vert", "shaders/vegetation.frag"));
    renderer->getVegetationShader()->use();
    renderer->getVegetationShader()->setInt("blockTextures", 0);
}


void App::render() {

	while (!glfwWindowShouldClose(window)) {

		if (interrupted().load(std::memory_order_relaxed))
			glfwSetWindowShouldClose(window, true);

		// Rotate query index each frame
		currentQueryIndex = (currentQueryIndex + 1) % QUERY_POOL_SIZE;

        // Skip rendering if window is minimized
        if (screenWidth == 0 || screenHeight == 0) {
            glfwPollEvents();
            continue;
        }

        // Calculate delta time for frame rate
        const float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

		// Menu rendering path (non-Playing states)
		if (gameState != GameState::Playing) {
			if (connectPending && udpClient) {
				udpClient->receivePacket();
				if (clientConnected) {
					connectPending = false;
					multiplayerMenu->clearError();
					transitionTo(GameState::Playing);
				} else if (glfwGetTime() - connectStartTime > connectTimeoutSec) {
					connectPending = false;
					udpClient.reset();
					clientConnected = false;
					multiplayerMenu->setErrorMessage("Connection timed out.");
				}
			}

			glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			ImGui_ImplOpenGL3_NewFrame();
			ImGui_ImplGlfw_NewFrame();
			ImGui::NewFrame();
			ImGui::Render();
			ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

			auto manager = menuManager.lock();
			if (manager) manager->render();

			// Pause/keep music silent while in menus. camera/renderer may be null pre-spawn.
			if (audio && camera && renderer) audio->update(deltaTime, false, *camera, *renderer);

			glfwSwapBuffers(window);
			glfwPollEvents();
			continue;
		}

        if (renderer) renderer->resetDrawCallCount();

		if (camera)
			camera->updateSmoothing(deltaTime);
		if (camera && renderer)
			camera->updateThirdPersonCollision(*renderer, deltaTime);

		NetPlayerInputs inputs = buildPlayerInputsPacket();
		auto manager = menuManager.lock();

		//Tick logic
		float tickDuration = 1.0f / TPS; // 0.05s per tick
		static float accumulator = 0.0f;
		accumulator += deltaTime;

        int simulatedTicksThisFrame = 0;
        constexpr int kMaxSimulatedTicksPerFrame = 6;
        std::vector<NetPlayerInputs> frameInputs;
        frameInputs.reserve(kMaxSimulatedTicksPerFrame);
        while (accumulator >= tickDuration && simulatedTicksThisFrame < kMaxSimulatedTicksPerFrame)
        {
            NetPlayerInputs tickInputs = inputs;
            if (manager)
                tickInputs.keys = 0;


            tickInputs.serverClientReconciliationTick = clientTick;
            camera->queueInput(tickInputs, clientTick);

            // Collect every tick's input; all will be sent as a batch so the
            // server can run one physics step per entry during catch-up.
            frameInputs.push_back(tickInputs);

            camera->predict(*renderer, clientTick);

            clientTime = clientTick * tickDuration;
            accumulator -= tickDuration;
            clientTickChangedTime = glfwGetTime();
            clientTick++;
            simulatedTicksThisFrame++;
        }

        // Send all inputs for this frame in one datagram.
        if (!frameInputs.empty()) {
            if (frameInputs.size() == 1) {
                udpClient->sendPacket(frameInputs[0]);
            } else {
                NetPacketGroup group;
                for (auto& inp : frameInputs)
                    group.add(inp);
                udpClient->sendPacket(group);
            }
        }

        if (simulatedTicksThisFrame == kMaxSimulatedTicksPerFrame && accumulator > tickDuration * 2.0f)
            accumulator = tickDuration * 2.0f;

        camera->setRenderTickAlpha(accumulator / tickDuration);
		udpClient->reliabilityKeepalive();
		udpClient->receivePacket();
        camera->flushPendingSnapshot(*renderer, clientTick);

        // Per-frame audio update: refresh listener pose, drive music + footstep triggers.
        if (audio) audio->update(deltaTime, gameState == GameState::Playing, *camera, *renderer);

        if (clientConnected && udpClient) {
            float now = static_cast<float>(glfwGetTime());
            if (now - lastPingSentTime >= 2.0f) {
                lastPingSentTime = now;
                if (serverIp == "127.0.0.1" || serverIp == "localhost") {
                    // Localhost: skip RTT measurement, set to 0 and report to server
                    pingMs = 0.0f;
                    NetPlayerPing report;
                    report.pingMs = 0.0f;
                    udpClient->sendPacket(report);
                }
                else {
                    auto ts = static_cast<uint64_t>(
                        std::chrono::duration_cast<std::chrono::microseconds>(
                            std::chrono::steady_clock::now().time_since_epoch()).count());
                    lastPingTimestamp = ts;
                    NetPing ping;
                    ping.timestamp = ts;
                    udpClient->sendPacket(ping);
                }
            }
        }

		//for some reason mouse needs a little delay to be put to false otherwise it glitches.
		if ((glfwGetTime() - lastMouseMoveTime) > tickDuration * 2)
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

		if (manager == inventoryUI)
		{
			NetInventoryAction pkt;
			if (inventoryUI->checkInventoryDrag(pkt))
				udpClient->sendPacket(pkt);
		}

        // window aspect / uniforms
        const float aspect = static_cast<float>(screenWidth) / static_cast<float>(screenHeight);

        glm::mat4 view = camera->getViewMatrix();
        glm::mat4 projection = glm::perspective(glm::radians(80.0f), aspect, 0.1f, renderDistance);
		glm::vec4 clipPlane = glm::vec4(0, -1, 0, 100000);  // No clipping

        // Update camera frustum for chunk culling (once per frame, before any render call)
     glm::mat4 viewRot = view;
        viewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
        renderer->updateFrustum(projection * viewRot, camera->getEyePosD());


        lighting->setViewportSize(screenWidth, screenHeight);
        lighting->updateSunDirection(deltaTime);
        lighting->updateSkyLUT(camera->getPlayer()->getPosition().y);


        renderer->processMeshUpdates();

        if (lighting->isShadowsEnabled() && lighting->isSunAboveHorizon()) {
            glBeginQuery(GL_TIME_ELAPSED, queryDrawShadowsPool[currentQueryIndex]);

         lighting->updateCSMShadowMaps(*renderer, view, camera->getEyePosD(), textureManager);

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
            lighting->uploadLightingUniforms(*textureShader, camera->getEyePosD(), camera->getPlayer()->getCameraDir());
            glActiveTexture(GL_TEXTURE0);
            textureManager.bind(GL_TEXTURE0);
            renderer->render(textureShader, view, camera->getEyePosD());
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
            lighting->uploadLightingUniforms(*textureShader, camera->getEyePosD(), camera->getPlayer()->getCameraDir());
            glActiveTexture(GL_TEXTURE0);
            textureManager.bind(GL_TEXTURE0);
            renderer->render(textureShader, view, camera->getEyePosD());
            renderTypeFramebuffer->unbindCurrentFrameBuffer();
        }

        // Restore main renderType for normal scene rendering
        if (textureShader) {
            textureShader->use();
            textureShader->setInt("renderType", selectedRenderType); // Normal lighting mode
        }

        // GBuffer pass
        glBeginQuery(GL_TIME_ELAPSED, queryGBufferPool[currentQueryIndex]);
        if (ssao && ssao->isEnabled()) {
            gBuffer->resize(screenWidth, screenHeight);
            ssao->resize(screenWidth, screenHeight);

            gBuffer->bind();
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

            gBufferShader->use();
            gBufferShader->setMat4("view", view);
            gBufferShader->setMat4("projection", projection);
            gBufferShader->setBool("useAlphaTest",
                ChunkRenderer::sLeafRenderMode != ChunkRenderer::LeafRenderMode::Fast);
            textureManager.bind(GL_TEXTURE0);
            renderer->render(gBufferShader, view, camera->getEyePosD(), false);

            gBuffer->unbind();
        }
        glEndQuery(GL_TIME_ELAPSED);

        // SSAO pass
        if (ssao && ssao->isEnabled()) {
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

		static float waterMoveOffset  = waterRenderer->getWaterMoveFactor();
		static float waterMoveOffset2 = waterRenderer->getWaterMoveFactor2();
		const float scrollSpeed1 = 0.012f; // ~83s per cycle
		const float scrollSpeed2 = 0.0078f; // ~128s per cycle (incommensurate)
		waterMoveOffset  += scrollSpeed1 * deltaTime;
		waterMoveOffset2 += scrollSpeed2 * deltaTime;
		if (waterMoveOffset  > 1.0f) waterMoveOffset  -= 1.0f;
		if (waterMoveOffset2 > 1.0f) waterMoveOffset2 -= 1.0f;
		waterRenderer->setWaterMoveFactor(waterMoveOffset);
		waterRenderer->setWaterMoveFactor2(waterMoveOffset2);
		// Non-wrapping wave phase — drives Gerstner displacement in the
		// vertex shader. Independent of waterMoveOffset (which wraps for
		// dudv UV scrolling).
		waterRenderer->advanceWaveTime(deltaTime);
		// Share the same phase clock with caustics so the ripples on
		// underwater terrain swim in lockstep with the surface waves.
		if (lighting) lighting->setCausticTime(waterRenderer->getWaveTime());

        glBeginQuery(GL_TIME_ELAPSED, queryDrawWaterReflectionPool[currentQueryIndex]);
        
        bool waterVisible = renderer->hasVisibleWater();
        if (waterVisible) {
            // Render reflection texture
            waterRenderer->renderWaterReflectionPass(activeShader, projection, textureManager);
        }

        glEndQuery(GL_TIME_ELAPSED);


        glBeginQuery(GL_TIME_ELAPSED, queryDrawWaterRefractionPool[currentQueryIndex]);
        if (waterVisible) {
            // render refraction texture
            waterRenderer->renderWaterRefractionPass(activeShader, view, projection, textureManager);
        }
        glEndQuery(GL_TIME_ELAPSED);    	
        
        const int currentChunkX = static_cast<int>(std::floor(camera->getPlayer()->getPosition().x / Chunk::WIDTH));
        const int currentChunkZ = static_cast<int>(std::floor(camera->getPlayer()->getPosition().z / Chunk::DEPTH));
        renderer->organizeChunks(Chunk::toKey(currentChunkX, currentChunkZ), camera->getPlayer()->getLoadRadius(), deltaTime);
        
        // --- Cloud march: renders to cloudFBO. renderCloudsLowRes() saves and
        // restores the caller's framebuffer + viewport internally, so the call
        // is order-independent w.r.t. sceneFBO.
        const glm::vec3 cameraEyeWorld = glm::vec3(camera->getEyePosD());
        glBeginQuery(GL_TIME_ELAPSED, queryDrawCloudsPool[currentQueryIndex]);
        lighting->renderCloudsLowRes(view, projection, cameraEyeWorld);
        glEndQuery(GL_TIME_ELAPSED);

        // --- Scene FBO (MSAA): sky, terrain, water, debug overlays ---
        // Lazy resize (mirrors the gBuffer/ssao pattern below).
        if (sceneFBO->getWidth() != screenWidth || sceneFBO->getHeight() != screenHeight)
            sceneFBO->resize(screenWidth, screenHeight);

        sceneFBO->bindMS();
        glViewport(0, 0, screenWidth, screenHeight);

        if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

    	// render to screen — pass useSSAO=false when GBuffer was skipped this frame
    	renderScene(view, projection, clipPlane);

    	// Render water with proper shader setup
        glBeginQuery(GL_TIME_ELAPSED, queryRenderWaterPool[currentQueryIndex]);
        const bool placedWaterVisible = renderer->hasVisiblePlacedWater();
        if (waterVisible || placedWaterVisible) {
            const float chunkDist = renderer->getMaxRenderedChunkDist();
            waterRenderer->setFogParams(fogEnabled, chunkDist * fogStartFraction, chunkDist, fogStrength);
            // Ocean surface — needs the planar reflection/refraction textures
            // produced by the passes above; only run when there's any to draw.
            if (waterVisible)
                waterRenderer->renderWaterSurface(projection);
            // Placed/spread water surface — sky-reflection shader, independent
            // of any global plane. Drawn after ocean so its own depth writes
            // sort against ocean fragments at the same Y.
            if (placedWaterVisible)
                waterRenderer->renderPlacedWaterSurface(projection);
        }
        glEndQuery(GL_TIME_ELAPSED);


		renderer->buildChunks();
        camera->drawWireframeSelectedBlockFace(renderer, view, projection);

        // Draw chunk boundary overlay (if enabled)
        chunkBoundaryRenderer->draw(camera->getPlayer()->getPosition(), camera->getEyePosD(), view, projection, *renderer);

        if (wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

        // Resolve MSAA -> non-MSAA textures, then composite clouds into the backbuffer.
        sceneFBO->resolve();
        SceneFramebuffer::unbind();
        glViewport(0, 0, screenWidth, screenHeight);

        // Auto-exposure: meter the resolved HDR scene (pre-clouds), advance the
        // smoothed exposure, push into Lighting so the cloud composite tonemap
        // (and any remaining LDR-fallback paths) use the same exposure value.
        if (hdrEnabled && autoExposureEnabled) {
            autoExposure->submit(sceneFBO->getResolvedColorTexture(), screenWidth, screenHeight);
            const float newExp = autoExposure->update(deltaTime, lighting->getSkyExposure());
            lighting->setSkyExposure(newExp);
        } else if (hdrEnabled) {
            // Manual exposure mode: just track the slider value.
            lighting->setSkyExposure(manualExposure);
        }

        // cameraEyeWorld must match the eye encoded in `view` — the composite
        // shader reconstructs the view ray via inverse(projection*view) and
        // computes (farWorld - cameraPosWorld). Mismatches also skew the
        // inside-layer check used to skip the depth-plane comparison.
        lighting->compositeCloudsToBackbuffer(
            sceneFBO->getResolvedColorTexture(),
            sceneFBO->getResolvedDepthTexture(),
            view, projection,
            cameraEyeWorld,
            glm::vec2(screenWidth, screenHeight)
        );

        glBindVertexArray(0);
        {
    		// Dynamically build GUI textures based on debug flags
    		guis.clear();
    		if (showReflectionTexture) {
    			guis.emplace_back(waterRenderer->getReflectionTexture(), glm::vec2(0.48f, 0.75f), glm::vec2(0.2f, 0.2f));
    		}
    		if (showRefractionTexture) {
    			guis.emplace_back(waterRenderer->getRefractionTexture(), glm::vec2(0.48f, 0.3f), glm::vec2(0.2f, 0.2f), true);
    		}
    		if (showRefractionDepthTexture) {
    			guis.emplace_back(waterRenderer->getRefractionDepthTexture(), glm::vec2(0.48f, -0.15f), glm::vec2(0.2f, 0.2f), true, true);
    		}
    		if (showNormalsTexture && renderTypeFramebuffer) {
    			guis.emplace_back(renderTypeFramebuffer->getNormalsTexture(), glm::vec2(0.05f, 0.75f), glm::vec2(0.2f, 0.2f), true);
    		}
    		if (showDepthTexture && renderTypeFramebuffer) {
    			guis.emplace_back(renderTypeFramebuffer->getDepthTexture(), glm::vec2(0.05f, 0.3f), glm::vec2(0.2f, 0.2f), true);
    		}

    		guiRenderer->render(guis, 0.1f, renderDistance);
        }

        computeDebugStats();

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
			// Death overlay sits above the hotbar/health but below the chat
			// recent-messages list so kill feed text stays readable.
			inventoryUI->drawDeathScreen(camera->getPlayer()->health);
			chat->renderRecentMessages();
		}

		if (showHUD) {
			debugHUD->update(cachedDebugStats);
			debugHUD->render();
		}

		if (playerListVisible && clientConnected) {
			std::vector<PlayerEntry> entries;
			entries.push_back({ localPlayerListId, settingsMenu->getUsername(), true, pingMs });
			for (auto& le : renderer->livingEntities) {
				if (!le || le->getLivingEntityType() != PLAYER) continue;
				if (le->getID() == localClientId) continue;
				float remPing = -1.0f;
				auto it = remotePings.find(le->getID());
				if (it != remotePings.end())
					remPing = it->second;
				uint32_t plId = 0;
				auto pit = entityToPlayerListId.find(le->getID());
				if (pit != entityToPlayerListId.end())
					plId = pit->second;
				std::string name = le->getName();
				entries.push_back({ plId, name, false, remPing });
			}
			playerListHUD->update(entries);
			playerListHUD->render();
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
            readGPUQueryEMA(queryDrawWaterRefractionPool[readIndex], measuredAverageMsDrawWaterRefraction, a);
            readGPUQueryEMA(queryGBufferPool[readIndex], measuredAverageMsGBuffer, a);
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

void App::renderScene(const glm::mat4 &view, const glm::mat4 &projection, const glm::vec4 clipPlane) const {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Render sky/clouds first with proper depth
    glDisable(GL_CLIP_DISTANCE0);


	glm::vec3 camPos = glm::inverse(camera->getViewMatrix())[3]; // Extract camera world position from view matrix
    const bool cameraUnderwater = renderer->isUnderwater(camPos);

    glBeginQuery(GL_TIME_ELAPSED, queryDrawSkyPool[currentQueryIndex]);
    lighting->drawSky(view, projection, camera->getPlayer()->getPosition(), cameraUnderwater);
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
   lighting->uploadLightingUniforms(*activeShader, camera->getEyePosD(), camera->getPlayer()->getCameraDir());
    uploadActiveSpotLights(*activeShader);
    lighting->uploadUnderwaterUniforms(*activeShader);
    activeShader->setBool("cameraUnderwater", cameraUnderwater);
    lighting->uploadCSMUniforms(*activeShader, view);

    // Bind SSAO texture for the lighting shader (must be after activeShader->use())
    if (ssao && ssao->isEnabled()) {
        glActiveTexture(GL_TEXTURE0 + TextureUnits::SSAO);
        glBindTexture(GL_TEXTURE_2D, ssao->getSSAOTexture());
        activeShader->setInt("ssaoTexture", TextureUnits::SSAO);
        activeShader->setInt("ssaoEnabled", 1);
        activeShader->setVec2("screenSize", glm::vec2(screenWidth, screenHeight));
    } else {
        activeShader->setInt("ssaoEnabled", 0);
    }

    // Fog
    GLuint skyLUTTex = lighting->getSkyLUTTexture();
    const float maxChunkDist = renderer->getMaxRenderedChunkDist();
    const float fogEnd   = maxChunkDist;
    const float fogStart = maxChunkDist * fogStartFraction;
    uploadFogUniforms(*activeShader, fogEnabled, skyLUTTex,
                      lighting->getSkyExposure(), fogStart, fogEnd, fogStrength,
                      lighting->isHDREnabled());

    glActiveTexture(GL_TEXTURE0);
    textureManager.bind(GL_TEXTURE0);

    // Setup vegetation shader with same lighting as terrain
    if (const auto& vegShader = renderer->getVegetationShader()) {
        vegShader->use();
        vegShader->setVec4("clipPlane", clipPlane);
        vegShader->setMat4("view", view);
        vegShader->setMat4("projection", projection);
        // Vegetation fragments now use FragPosRel (camera-relative) for fog distances,
        // so viewPos is the origin of render space — vec3(0).
        vegShader->setVec3("viewPos", glm::vec3(0.0f));

        // Use the same day/night cycle as the main lighting system
        glm::vec3 sunDir = lighting->getDirectionalLightDirection();
        float sunElevation = sunDir.y;
        float day = glm::clamp(sunElevation * 2.0f, 0.0f, 1.0f);
        day = glm::smoothstep(0.0f, 1.0f, day);

        // Matches the value in Lighting::uploadLightingUniforms — kept in sync
        // so terrain and vegetation share the same night-time floor.
        constexpr float nightAmbientMin = 0.05f;
        glm::vec3 ambientColor = lighting->getDirectionalAmbientColor() * (nightAmbientMin + (1.0f - nightAmbientMin) * day);
        glm::vec3 diffuseColor = lighting->getDirectionalDiffuseColor() * day;

        vegShader->setVec3("lightDir", -sunDir);
        vegShader->setVec3("lightColor", diffuseColor);
        vegShader->setVec3("ambientColor", ambientColor);
        vegShader->setFloat("time", static_cast<float>(glfwGetTime()));
        vegShader->setFloat("seaLevel", 64.0f);

        // Graphics-quality knobs (sway tier, distance LOD, density skip)
        vegShader->setInt  ("vegetationSwayQuality",  renderer->getVegetationSwayQuality());
        vegShader->setFloat("vegetationSwayMaxDist",  renderer->getVegetationSwayMaxDistance());
        vegShader->setInt  ("vegetationDensity",      renderer->getVegetationDensity());

        // Underwater fog for vegetation
        vegShader->setBool("cameraUnderwater", cameraUnderwater);
    	vegShader->setVec3("underwaterTintColor", lighting->getUnderwaterTintColor());
    	vegShader->setVec3("underwaterFogColor", lighting->getUnderwaterFogColor());
    	vegShader->setFloat("underwaterFogDensity", lighting->getUnderwaterFogDensity());

        // Upload CSM shadow uniforms to vegetation shader
        lighting->uploadCSMUniforms(*vegShader, view);
        vegShader->setInt("shadowsEnabled", lighting->isShadowsEnabled());

        // Fog for vegetation
        uploadFogUniforms(*vegShader, fogEnabled, skyLUTTex,
                          lighting->getSkyExposure(), fogStart, fogEnd, fogStrength,
                          lighting->isHDREnabled());

        activeShader->use(); // Switch back to main shader
    }

    // Derive the alpha-test flag from the current leaf-render mode.
    // Fast leaves are opaque (no discard); Fancy/Smart keep the cutout test.
    const bool leafAlphaTest =
        (ChunkRenderer::sLeafRenderMode != ChunkRenderer::LeafRenderMode::Fast);
    activeShader->use();
    activeShader->setBool("useAlphaTest", leafAlphaTest);

    glBeginQuery(GL_TIME_ELAPSED, queryRenderShaderPool[currentQueryIndex]);
    if (depthPrepassEnabled) {
        // ── Z-prepass ────────────────────────────────────────────────
        // Render terrain depth-only with color writes off. Subsequent color
        // pass uses GL_EQUAL so each pixel only runs the heavy lighting
        // shader once, regardless of overdraw — big win in dense jungle.
        depthPrepassShader->use();
        depthPrepassShader->setMat4("projection", projection);
        depthPrepassShader->setVec4("clipPlane", clipPlane);
        depthPrepassShader->setBool("useAlphaTest", leafAlphaTest);
        textureManager.bind(GL_TEXTURE0); // for alpha test on leaves
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        renderer->renderTerrainOnly(depthPrepassShader, view, camera->getEyePosD());
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);

        // ── Color pass: terrain only, GL_EQUAL ────────────────────────
        glDepthFunc(GL_EQUAL);
        glDepthMask(GL_FALSE);  // prepass already wrote depth; no need to write it again
        // Tiny tweak: with prepass, the alpha discard in the color shader
        // is redundant (same texels were already discarded in prepass).
        // Leaving it in is harmless and avoids a separate shader variant.
        renderer->renderTerrainOnly(activeShader, view, camera->getEyePosD());
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

        // ── Vegetation pass (LESS depth, no prepass) ──────────────────
        renderer->renderVegetationOnly(view, camera->getEyePosD());
        activeShader->use();
    } else {
        renderer->render(activeShader, view, camera->getEyePosD());
    }
    glEndQuery(GL_TIME_ELAPSED);

    lighting->drawLightCubes(view, projection, camera->getEyePosD());

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
	// Upload the same directional/point/shadow uniforms terrain uses so dropped
	// items react to point lights, get shadowed by CSM, and dim at night.
	// entity_lighting.glsl reads the exact same uniform names lighting.frag does.
	{
		Shader& propShader = m_itemPropEntityManager->getShader();
		lighting->uploadLightingUniforms(propShader, camera->getEyePosD(), camera->getPlayer()->getCameraDir());
		uploadActiveSpotLights(propShader);
		lighting->uploadCSMUniforms(propShader, view);
	}
	// Pass the renderer (a CommonWorld<ChunkRenderer>) so updateMesh can sample
	// per-item sky-light and bake it into the vertex stream , uniform won't
	// work here because every item in the world is one batched draw call.
	m_itemPropEntityManager->draw(projection, view, camera->getEyePosD(), renderer->itemEntities, renderer.get());
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
     if (!entity->snapshots.empty() && entity->getPositionD() == entity->snapshots.back().position && !firstFrame) {
            entity->positionUpdated = false; 
        }
		firstFrame = false;
	}

	// Sync the local player's mesh position with the interpolated camera target
	// so the character doesn't shake in third-person due to the prediction/
	// reconciliation cycle updating the raw physics position mid-frame.
	auto &localPlayer = *camera->getPlayer();
	if (camera->isThirdPersonCameraActive()) {
		localPlayer.renderPos = camera->getInterpolatedPlayerPosD();
		localPlayer.hasRenderPos = true;
	} else {
		localPlayer.hasRenderPos = false;
	}

	{
		const bool dead = localPlayer.health <= 0.0f;
		auto &bp = localPlayer.characterBodyParts;
		if (dead && !bp.dying) {
			localPlayer.triggerDeath();
		} else if (!dead && bp.dying) {
			bp.dying = false;
			bp.dyingDone = false;
			bp.dyingPhase = 0.0f;
			localPlayer.removed = false; // manager flips this on dyingDone
		}
	}

  // Upload the same lighting+CSM uniforms the terrain uses so mobs/players
  // receive directional light, point lights, and CSM shadows just like the
  // world they're standing in.
  {
      Shader& chShader = renderer->livingEntitiesManager.getShader();
      lighting->uploadLightingUniforms(chShader, camera->getEyePosD(), camera->getPlayer()->getCameraDir());
      uploadActiveSpotLights(chShader);
      lighting->uploadCSMUniforms(chShader, view);
  }
  renderer->drawCharacters(projection, view, camera->getEyePosD(), deltaTime);
}

void App::uploadActiveSpotLights(Shader& shader) const
{
    std::vector<Lighting::SpotLightUpload> lights;
    if (!lighting) return;

    // Slot 0: local flashlight (camera-attached) if on.
    if (lighting->isFlashlightOn() && camera) {
        lights.push_back({ glm::vec3(0.0f), camera->getPlayer()->getCameraDir() });
    }

    // Then every remote player whose flashlight is on, sorted by distance so
    // the nearest ones win if we overflow MAX_SPOT_LIGHTS.
    if (renderer && camera) {
        const glm::dvec3 eyePos = camera->getEyePosD();
        struct RemoteHit { float d2; glm::vec3 posRel; glm::vec3 dir; };
        std::vector<RemoteHit> remotes;
        remotes.reserve(4);
        for (auto& le : renderer->livingEntities) {
            if (!le || le->getLivingEntityType() != PLAYER) continue;
            if (!le->flashlightOn) continue;
            // Local player entity is tagged with id == -1 (see LivingEntitiesManager).
            if (le->getID() == static_cast<entityID>(-1)) continue;

            glm::vec3 posRel = glm::vec3(le->getPositionD() - eyePos);
            posRel.y += static_cast<float>(le->getEntityHeight()) * 0.9f;

            // Look direction from yaw/pitch — mirrors PlayerMovement::updateCameraVectors.
            const float yr = glm::radians(le->yaw);
            const float pr = glm::radians(le->pitch);
            glm::vec3 dir = glm::normalize(glm::vec3(
                std::cos(yr) * std::cos(pr),
                std::sin(pr),
                std::sin(yr) * std::cos(pr)
            ));
            remotes.push_back({ glm::dot(posRel, posRel), posRel, dir });
        }
        std::sort(remotes.begin(), remotes.end(),
                  [](const RemoteHit& a, const RemoteHit& b) { return a.d2 < b.d2; });
        for (auto& r : remotes) {
            if (static_cast<int>(lights.size()) >= Lighting::MAX_SPOT_LIGHTS) break;
            lights.push_back({ r.posRel, r.dir });
        }
    }

    lighting->uploadSpotLights(shader, lights);
}

void App::computeDebugStats()
{
    cachedDebugStats.fps = uiDisplayFPS;

	cachedDebugStats.cpuFrameMs = deltaTime * 1000.0f;

	cachedDebugStats.pingMs = clientConnected ? pingMs : -1.0f;

    if (renderer) {
        cachedDebugStats.visibleChunks = renderer->getVisibleChunkCount();
        cachedDebugStats.totalChunks   = renderer->getTotalChunkCount();

        size_t solidVerts = 0;
        size_t waterVerts = 0;
        for (auto& weakChunk : renderer->getRenderedChunks()) {
            if (auto chunk = weakChunk.lock()) {
                solidVerts += chunk->getMeshVertexCount();
                waterVerts += chunk->getWaterMeshVertexCount();
            }
        }
        const size_t totalVerts      = solidVerts + waterVerts;
        cachedDebugStats.triangles   = totalVerts / 3;
        cachedDebugStats.cubes       = cachedDebugStats.triangles / 12;

		cachedDebugStats.terrainDrawCalls = renderer->getDrawCallCount();

        if (camera && camera->getPlayer())
            cachedDebugStats.playerPos = camera->getPlayer()->getPosition();
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
                style._NextFrameFontSizeBase = 30.0f;
                appliedDefaultFontSize = true;
            }

            glm::vec3 pos = camera->getPlayer()->getPosition();
            double wx = static_cast<double>(std::floor(pos.x));
            double wz = static_cast<double>(std::floor(pos.z));
            int wy = static_cast<int>(std::floor(pos.y));
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_AlwaysAutoResize;
            if (!uiInteractive) {
                flags |= ImGuiWindowFlags_NoInputs;
                // Make the overlay slightly transparent when not interactive
                ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.6f);
            }

            // Color helpers
            auto fpsColor = [](float fps) -> ImVec4 {
                if (fps >= 60.0f) return ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
                if (fps >= 30.0f) return ImVec4(0.9f, 0.8f, 0.1f, 1.0f);
                return ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
            };
            auto pingColor = [](float ping) -> ImVec4 {
                if (ping < 50.0f)  return ImVec4(0.2f, 0.9f, 0.2f, 1.0f);
                if (ping < 150.0f) return ImVec4(0.9f, 0.8f, 0.1f, 1.0f);
                return ImVec4(0.9f, 0.2f, 0.2f, 1.0f);
            };

            ImGui::Begin("Debug Window", nullptr, flags);

            ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
            if (ImGui::BeginTabBar("Tabs", tab_bar_flags))
            {
                // ── Overview ──────────────────────────────────────────────
                if (ImGui::BeginTabItem("Overview"))
                {
                    // Performance
                    ImGui::SeparatorText("Performance");
                    ImGui::TextColored(fpsColor(uiDisplayFPS), "FPS: %.1f (%.3f ms)",
                        uiDisplayFPS, uiDisplayFPS > 0.0f ? 1000.0f / uiDisplayFPS : 0.0f);
                    if (clientConnected)
                        ImGui::TextColored(pingColor(pingMs), "Ping: %.0f ms", pingMs);
                    else
                        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Ping: offline");
                    {
                        const size_t memBytes = getCurrentRSS();
                        const double memMB = memBytes / (1024.0 * 1024.0);
                        ImVec4 memColor = memMB > 2048.0 ? ImVec4(0.9f, 0.2f, 0.2f, 1.0f)
                                        : memMB > 1024.0 ? ImVec4(0.9f, 0.8f, 0.1f, 1.0f)
                                                         : ImGui::GetStyleColorVec4(ImGuiCol_Text);
                        ImGui::TextColored(memColor, "Memory: %.2f MB", memMB);
                    }

                    // Quick Toggles
                    ImGui::SeparatorText("Quick Toggles");
                    auto quickToggle = [&](const char* label, bool active, auto onToggle) {
                        ImGui::PushStyleColor(ImGuiCol_Button,
                            active ? ImVec4(0.2f, 0.6f, 0.2f, 1.0f) : ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
                        if (ImGui::Button(label, ImVec2(110, 0)))
                            onToggle(!active);
                        ImGui::PopStyleColor();
                    };
                    {
                        bool shadowsEnabled = lighting->isShadowsEnabled();
                        bool ssaoEnabled    = ssao->isEnabled();
                        bool flashlightOn   = lighting->isFlashlightOn();
                        quickToggle("Wireframe", wireframe, [&](bool v) {
                            wireframe = v;
                        });
                        ImGui::SameLine();
                        quickToggle("V-Sync", vsync, [&](bool v) {
                            vsync = v;
                            glfwSwapInterval(vsync ? 1 : 0);
                        });
                        ImGui::SameLine();
                        quickToggle("Shadows", shadowsEnabled, [&](bool v) { lighting->setShadowsEnabled(v); });
                        ImGui::SameLine();
                        quickToggle("SSAO", ssaoEnabled, [&](bool v) { ssao->setEnabled(v); });
                        ImGui::SameLine();
                        quickToggle("Flashlight", flashlightOn, [&](bool v) { lighting->setSpotLightOn(v); });
                    }

                    // World
                    ImGui::SeparatorText("World");
                    ImGui::Text("Position:  x=%f  y=%d  z=%f", wx, wy, wz);
                    ImGui::Text("Seed: %d", currentWorldSeed);
                    ImGui::Text("Height: %d  (Sea Level: %d)", currentTerrainHeight, currentSeaLevel);
                    {
                        const char* biomeName =
                            (static_cast<BiomeType>(currentBiome) == BiomeType::PLAINS)          ? "PLAINS" :
                            (static_cast<BiomeType>(currentBiome) == BiomeType::DESERT)          ? "DESERT" :
                            (static_cast<BiomeType>(currentBiome) == BiomeType::DARK_FOREST)     ? "DARK_FOREST" :
                            (static_cast<BiomeType>(currentBiome) == BiomeType::TUNDRA)          ? "TUNDRA" :
                            (static_cast<BiomeType>(currentBiome) == BiomeType::SWAMP)           ? "SWAMP" :
                            (static_cast<BiomeType>(currentBiome) == BiomeType::OCEAN)           ? "OCEAN" :
                            (static_cast<BiomeType>(currentBiome) == BiomeType::MOUNTAIN)        ? "MOUNTAIN" :
                            (static_cast<BiomeType>(currentBiome) == BiomeType::BIRCH_FOREST)    ? "BIRCH_FOREST" :
                            (static_cast<BiomeType>(currentBiome) == BiomeType::JUNGLE)          ? "JUNGLE" :
                            (static_cast<BiomeType>(currentBiome) == BiomeType::SAVANNA)         ? "SAVANNA" :
                            (static_cast<BiomeType>(currentBiome) == BiomeType::MESA)            ? "MESA" :
                            (static_cast<BiomeType>(currentBiome) == BiomeType::ICE_PLAINS)      ? "ICE_PLAINS" :
                            (static_cast<BiomeType>(currentBiome) == BiomeType::VOLCANIC)        ? "VOLCANIC" :
                            (static_cast<BiomeType>(currentBiome) == BiomeType::RED_DESERT)      ? "RED_DESERT" :
                            (static_cast<BiomeType>(currentBiome) == BiomeType::NETHER)          ? "NETHER" :
                            (static_cast<BiomeType>(currentBiome) == BiomeType::MUSHROOM_ISLAND) ? "MUSHROOM_ISLAND" :
                                                                                                   "UNKNOWN";
                        ImGui::Text("Biome: %s", biomeName);
                    }

                    // Teleport (collapsible)
                    if (ImGui::CollapsingHeader("Teleport")) {
                        static double tpX = 5000000;
                        static int tpY = 100;
                        static double tpZ = 0;

                        // Negative width = "extend to N pixels from the right edge",
                        // so the field grows/shrinks with the window while leaving
                        // room for the label and the +/- steppers.
                        const float tpFieldTrailing = -60.0f;
                        ImGui::SetNextItemWidth(tpFieldTrailing);
                        ImGui::InputDouble("X##tp", &tpX);
                        ImGui::SetNextItemWidth(tpFieldTrailing);
                        ImGui::InputInt("Y##tp", &tpY);
                        ImGui::SetNextItemWidth(tpFieldTrailing);
                        ImGui::InputDouble("Z##tp", &tpZ);

                        if (ImGui::Button("Copy current")) {
                            tpX = wx; tpY = wy; tpZ = wz;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Teleport##tp")) {
                            if (udpClient) {
                                NetMessage cmd;
                                cmd.message = "/tp " +
                                              std::to_string(tpX) + " " +
                                              std::to_string(tpY) + " " +
                                              std::to_string(tpZ);
                                udpClient->sendPacket(cmd);
                            }
                        }
                    }

                    // Noise Values (collapsible)
                    if (ImGui::CollapsingHeader("Noise Values", ImGuiTreeNodeFlags_DefaultOpen)) {
                        static const char* contBucketNames[]    = { "MUSHROOM", "OCEAN", "COAST", "NEAR_INLAND", "MID_INLAND", "FAR_INLAND" };
                        static const char* erosionBucketNames[] = { "E0", "E1", "E2", "E3", "E4", "E5", "E6" };
                        static const char* pvBucketNames[]      = { "VALLEY", "LOW", "MID", "HIGH", "PEAK" };
                        static const char* tempBucketNames[]    = { "VERY_COLD", "COLD", "TEMPERATE", "WARM", "HOT" };
                        static const char* humidBucketNames[]   = { "ARID", "DRY", "NEUTRAL", "HUMID", "WET" };
                        ImGui::Text("Continentalness: %.3f", currentContinentalness); ImGui::SameLine(); ImGui::TextDisabled("(%s)", contBucketNames[currentContBucket]);
                        ImGui::Text("Erosion:         %.3f", currentErosion);         ImGui::SameLine(); ImGui::TextDisabled("(%s)", erosionBucketNames[currentErosionBucket]);
                        ImGui::Text("Peak/Valley:     %.3f", currentPeakValley);      ImGui::SameLine(); ImGui::TextDisabled("(%s)", pvBucketNames[currentPVBucket]);
                        ImGui::Text("Temperature:     %.3f", currentTemperature);     ImGui::SameLine(); ImGui::TextDisabled("(%s)", tempBucketNames[currentTempBucket]);
                        ImGui::Text("Humidity:        %.3f", currentHumidity);        ImGui::SameLine(); ImGui::TextDisabled("(%s)", humidBucketNames[currentHumidBucket]);
                    }

                    // Renderer stats
                    if (renderer) {
                        // TODO: fix real count based on frustum culling
                        ImGui::SeparatorText("Renderer");
                        ImGui::Text("Chunks: %zu / %zu", cachedDebugStats.visibleChunks, cachedDebugStats.totalChunks);
                        ImGui::Text("Triangles: %zu", cachedDebugStats.triangles);
                        ImGui::Text("Visible Blocks: %zu", cachedDebugStats.cubes);
                        ImGui::Text("Draw Calls: %zu", cachedDebugStats.terrainDrawCalls);
                    }

                    ImGui::Separator();

                    // Heightmap generator
                    if (ImGui::CollapsingHeader("Heightmap")) {
                        ImGui::Text("Heightmap Generation (server-side)");
                        ImGui::InputInt("Size ([1-1024])", &debugTerrainParams.genSize);
                        ImGui::InputInt("Downsample ([1-256])", &debugTerrainParams.downsample);
                        debugTerrainParams.genSize    = std::max(1, debugTerrainParams.genSize);
                        debugTerrainParams.downsample = std::max(1, debugTerrainParams.downsample);
                        
                        auto sendDumpCommand = [&](const char* mode) {
                            if (!udpClient) return;
                            NetMessage cmd;
                            cmd.message = std::string("/dump ") + mode + " " +
                                          std::to_string(debugTerrainParams.genSize) + " " +
                                          std::to_string(debugTerrainParams.downsample);
                            udpClient->sendPacket(cmd);
                        };
                        if (ImGui::Button("Generate Hydros"))     sendDumpCommand("hydro");
                        if (ImGui::Button("Generate Noises"))     sendDumpCommand("noises");
                        if (ImGui::Button("Generate Heightmaps")) sendDumpCommand("heightmap");
                        if (ImGui::Button("Generate Biome Map"))  sendDumpCommand("biome");
                    }

                    if (ImGui::CollapsingHeader("Network Debug")) {
                        const auto& netStats = camera->getReconcileDebugStats();
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
                        }
                        else {
                            ImGui::Text("Sim Latency: %.0f ms", udpClient->getSimulatedLatency());
                        }
                    }

                    ImGui::EndTabItem();
                }

                if (ImGui::BeginTabItem("Graphics Quality")) {
                    // Preset buttons — apply shadow + water + vegetation + SSAO together.
                    if (ImGui::Button("Performance")) {
                        // Shadows
                        lighting->setCascadeCount(2);
                        lighting->setShadowMapResolution(1024);
                        lighting->setShadowFarPlane(250.0f);
                        lighting->setPcfQuality(Lighting::PcfQuality::Low);
                        lighting->setShadowAlphaTest(false);
                        // Water — disable reflection, half-res refraction without vegetation
                        waterRenderer->setReflectionEnabled(false);
                        waterRenderer->setRefractionResolutionScale(0.5f, screenWidth, screenHeight);
                        waterRenderer->setRefractionVegetationEnabled(false);
                        waterRenderer->setReflectionMaxDistance(0.0f);
                        // Vegetation distance limiter (jungle scenes)
                        renderer->setVegetationMaxDistance(100.0f);
                        // Vegetation: no sway, half density — biggest jungle win
                        renderer->setVegetationSwayQuality(0);
                        renderer->setVegetationSwayMaxDistance(0.0f);
                        renderer->setVegetationDensity(2);
                        depthPrepassEnabled = true;
                        // Performance: solid-cube leaves, no alpha test.
                        ChunkRenderer::sLeafRenderMode = ChunkRenderer::LeafRenderMode::Fast;
                        for (auto &cp : renderer->getRenderedChunks()) if (auto c = cp.lock()) c->buildMesh();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Balanced")) {
                        lighting->setCascadeCount(3);
                        lighting->setShadowMapResolution(2048);
                        lighting->setShadowFarPlane(350.0f);
                        lighting->setPcfQuality(Lighting::PcfQuality::Medium);
                        lighting->setShadowAlphaTest(false);
                        waterRenderer->setReflectionEnabled(true);
                        waterRenderer->setRefractionResolutionScale(0.5f, screenWidth, screenHeight);
                        waterRenderer->setRefractionVegetationEnabled(true);
                        waterRenderer->setReflectionMaxDistance(120.0f);
                        renderer->setVegetationMaxDistance(200.0f);
                        // Cheap sway, full density, fade out near the cull distance
                        renderer->setVegetationSwayQuality(1);
                        renderer->setVegetationSwayMaxDistance(150.0f);
                        renderer->setVegetationDensity(1);
                        depthPrepassEnabled = true;
                        // Balanced: keep leaf cutouts on outer surfaces, skip internal faces.
                        ChunkRenderer::sLeafRenderMode = ChunkRenderer::LeafRenderMode::Smart;
                        for (auto &cp : renderer->getRenderedChunks()) if (auto c = cp.lock()) c->buildMesh();
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("High")) {
                        lighting->setCascadeCount(3);
                        lighting->setShadowMapResolution(2048);
                        lighting->setShadowFarPlane(500.0f);
                        lighting->setPcfQuality(Lighting::PcfQuality::High);
                        lighting->setShadowAlphaTest(true);
                        waterRenderer->setReflectionEnabled(true);
                        waterRenderer->setRefractionResolutionScale(1.0f, screenWidth, screenHeight);
                        waterRenderer->setRefractionVegetationEnabled(true);
                        waterRenderer->setReflectionMaxDistance(0.0f);
                        renderer->setVegetationMaxDistance(0.0f);
                        // Full sway, no LOD, full density
                        renderer->setVegetationSwayQuality(2);
                        renderer->setVegetationSwayMaxDistance(0.0f);
                        renderer->setVegetationDensity(1);
                        depthPrepassEnabled = true;
                        // High: original look — leaves transparent, every face emitted.
                        ChunkRenderer::sLeafRenderMode = ChunkRenderer::LeafRenderMode::Fancy;
                        for (auto &cp : renderer->getRenderedChunks()) if (auto c = cp.lock()) c->buildMesh();
                    }

                    ImGui::SeparatorText("Shadow Map");
                    static const int kRes[] = { 512, 1024, 2048, 4096 };
                    int curResIdx = 1;
                    for (int i = 0; i < 4; ++i) if ((int)lighting->getShadowMapResolution() == kRes[i]) curResIdx = i;
                    if (ImGui::Combo("Resolution", &curResIdx, "512\0001024\0002048\0004096\000"))
                        lighting->setShadowMapResolution(kRes[curResIdx]);

                    int cascades = lighting->getCascadeCount();
                    if (ImGui::SliderInt("Cascades", &cascades, 2, 3))
                        lighting->setCascadeCount(cascades);

                    float farP = lighting->getShadowFarPlane();
                    if (ImGui::SliderFloat("Shadow Distance", &farP, 100.0f, 1000.0f, "%.0f"))
                        lighting->setShadowFarPlane(farP);

                    ImGui::SeparatorText("Filtering");
                    int pcf = static_cast<int>(lighting->getPcfQuality());
                    if (ImGui::Combo("PCF Quality", &pcf, "Low (1 tap)\0Medium (3x3)\0High (5x5)\0"))
                        lighting->setPcfQuality(static_cast<Lighting::PcfQuality>(pcf));

                    bool alpha = lighting->getShadowAlphaTest();
                    if (ImGui::Checkbox("Leaf shadows (alpha-tested)", &alpha))
                        lighting->setShadowAlphaTest(alpha);

                    ImGui::SeparatorText("Water");
                    bool reflEnabled = waterRenderer->isReflectionEnabled();
                    if (ImGui::Checkbox("Water reflection", &reflEnabled))
                        waterRenderer->setReflectionEnabled(reflEnabled);

                    float reflMax = waterRenderer->getReflectionMaxDistance();
                    if (ImGui::SliderFloat("Reflection distance (0 = no cap)", &reflMax, 0.0f, 500.0f, "%.0f"))
                        waterRenderer->setReflectionMaxDistance(reflMax);

                    float refrScale = waterRenderer->getRefractionResolutionScale();
                    if (ImGui::SliderFloat("Refraction resolution", &refrScale, 0.25f, 1.0f, "%.2fx"))
                        waterRenderer->setRefractionResolutionScale(refrScale, screenWidth, screenHeight);

                    bool refrVeg = waterRenderer->isRefractionVegetationEnabled();
                    if (ImGui::Checkbox("Vegetation in refraction", &refrVeg))
                        waterRenderer->setRefractionVegetationEnabled(refrVeg);

                    ImGui::SeparatorText("Vegetation");
                    float vegDist = renderer->getVegetationMaxDistance();
                    if (ImGui::SliderFloat("Vegetation distance (0 = no cap)", &vegDist, 0.0f, 400.0f, "%.0f"))
                        renderer->setVegetationMaxDistance(vegDist);

                    int swayQ = renderer->getVegetationSwayQuality();
                    if (ImGui::Combo("Wind sway quality", &swayQ,
                                     "None (cheapest)\0Low (1 sin)\0High (current)\0"))
                        renderer->setVegetationSwayQuality(swayQ);

                    float swayDist = renderer->getVegetationSwayMaxDistance();
                    if (ImGui::SliderFloat("Sway LOD distance (0 = no fade)", &swayDist, 0.0f, 300.0f, "%.0f"))
                        renderer->setVegetationSwayMaxDistance(swayDist);

                    int density = renderer->getVegetationDensity();
                    if (ImGui::SliderInt("Density (render every Nth)", &density, 1, 4))
                        renderer->setVegetationDensity(density);

                    ImGui::SeparatorText("Ambient Occlusion");
                    bool ssaoOn = ssao->isEnabled();
                    if (ImGui::Checkbox("SSAO", &ssaoOn))
                        ssao->setEnabled(ssaoOn);

                    ImGui::SeparatorText("Terrain");
                    ImGui::Checkbox("Z-prepass (kills fragment overdraw)", &depthPrepassEnabled);

                    ImGui::Separator();
                    ImGui::TextWrapped("Leaf rendering. Switching modes only affects new mesh builds.");
                    int leafMode = static_cast<int>(ChunkRenderer::sLeafRenderMode);
                    if (ImGui::Combo("Leaf mode", &leafMode,
                                     "Fast (opaque leaves, no cutouts)\0"
                                     "Fancy (transparent, all faces)\0"
                                     "Smart (cutouts, fewer faces)\0")) {
                        ChunkRenderer::sLeafRenderMode = static_cast<ChunkRenderer::LeafRenderMode>(leafMode);
                        for (auto &chunkPtr : renderer->getRenderedChunks())
                            if (auto chunk = chunkPtr.lock())
                                chunk->buildMesh();
                    }
                    ImGui::SetItemTooltip(
                        "Fast  — leaves render as solid green cubes. Mesher culls leaf-to-leaf and solid-to-leaf faces. Cheapest.\n"
                        "Fancy — original look: leaves are alpha-tested, every face emitted (you can see leaves through other leaves). Most expensive.\n"
                        "Smart — leaves keep alpha cutouts on outer faces, but mesher skips internal faces. Hollow canopies; recommended balance.");

                    if (ImGui::Button("Rebuild all chunk meshes")) {
                        for (auto &chunkPtr : renderer->getRenderedChunks())
                            if (auto chunk = chunkPtr.lock())
                                chunk->buildMesh();
                    }
                    ImGui::SameLine();
                    ImGui::TextDisabled("(or press F3+A)");

                    ImGui::EndTabItem();
                }

                // ── Rendering ────────────────────────────────────────────
                if (ImGui::BeginTabItem("Rendering"))
                {
                    if (ImGui::BeginTabBar("Rendering", tab_bar_flags))
                    {
                        if (ImGui::BeginTabItem("Options"))
                        {
                            if (ImGui::Checkbox("V-Sync", &vsync))
                                glfwSwapInterval(vsync ? 1 : 0);
                            ImGui::Checkbox("Wireframe", &wireframe);
                            ImGui::RadioButton("Lighting render", &selectedRenderType, 0); ImGui::SameLine();
                            ImGui::RadioButton("Normals render",  &selectedRenderType, 1); ImGui::SameLine();
                            ImGui::RadioButton("Depth render",    &selectedRenderType, 2);
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

							bool msaaEnabled = renderer->isMSAAEnabled();
							if (ImGui::Checkbox("MSAA", &msaaEnabled))
								renderer->setMSAAEnabled(msaaEnabled);
                            
                            ImGui::SliderFloat("Clipping plane Distance", &renderDistance, 100.0f, 2000.0f);
                            
                            if (renderer) {
                                int radius = camera->getPlayer()->getLoadRadius();
                                if (ImGui::SliderInt("Chunk Load Radius", &radius, 4, 32))
                                    camera->getPlayer()->setLoadRadius(radius);
                            }
                            {
                                bool cb = chunkBoundaryRenderer->isEnabled();
                                if (ImGui::Checkbox("Show Chunk Boundary", &cb))
                                    chunkBoundaryRenderer->setEnabled(cb);
                            }
                            // Toggle per-entity AABB outlines. off by default
                            ImGui::Checkbox("Show Entity Hitboxes",
                                            &renderer->livingEntitiesManager.showHitboxes);

                            ImGui::EndTabItem();
                        }
                        // ── HDR / Exposure ─────────────────────────────────────────
                        // Owns the toggles wired to sceneFBO::setHDR (RGBA16F<->RGBA8)
                        // and AutoExposure (PBO readback metering).
                        if (ImGui::BeginTabItem("HDR / Exposure"))
                        {
                            if (ImGui::Checkbox("HDR enabled", &hdrEnabled)) {
                                sceneFBO->setHDR(hdrEnabled);
                                lighting->setHDREnabled(hdrEnabled);
                                // Water reflection/refraction targets must match the scene's
                                // color space — otherwise HDR scene radiance gets clamped to
                                // [0,1] in those FBOs and water.frag then samples LDR values
                                // back into the HDR scene buffer.
                                if (waterRenderer)
                                    waterRenderer->setHDR(hdrEnabled);
                                // When flipping back to LDR, pull exposure back to a sane
                                // manual value so the cloud composite (LDR path) doesn't
                                // inherit a stale auto-exp value.
                                if (!hdrEnabled)
                                    lighting->setSkyExposure(manualExposure);
                            }
                            // Saturation works in either HDR or LDR mode — it's applied in
                            // display space at the end of clouds_composite. Default 1.2 to
                            // compensate for the tonemap's midtone desaturation when fed
                            // sRGB-encoded textures (see clouds_composite.frag).
                            {
                                float sat = lighting->getSkySaturation();
                                if (ImGui::SliderFloat("Saturation", &sat, 0.0f, 2.0f, "%.2f"))
                                    lighting->setSkySaturation(sat);
                            }
                            ImGui::BeginDisabled(!hdrEnabled);
                            ImGui::Checkbox("Auto-exposure", &autoExposureEnabled);
                            ImGui::BeginDisabled(autoExposureEnabled);
                            if (ImGui::SliderFloat("Manual exposure", &manualExposure, 0.3f, 4.0f, "%.2f"))
                                lighting->setSkyExposure(manualExposure);
                            ImGui::EndDisabled();
                            if (autoExposureEnabled && autoExposure) {
                                ImGui::SliderFloat("Target luminance", &autoExposure->targetLuminance, 0.05f, 0.4f, "%.3f");
                                ImGui::SliderFloat("Min exposure",     &autoExposure->minExposure,     0.05f, 1.0f, "%.2f");
                                ImGui::SliderFloat("Max exposure",     &autoExposure->maxExposure,     1.0f, 8.0f,  "%.2f");
                                ImGui::SliderFloat("Adapt up (s^-1)",   &autoExposure->adaptSpeedUp,   0.1f, 4.0f, "%.2f");
                                ImGui::SliderFloat("Adapt down (s^-1)", &autoExposure->adaptSpeedDown, 0.1f, 4.0f, "%.2f");
                                ImGui::Text("Current exposure: %.2f", lighting->getSkyExposure());
                                ImGui::Text("Avg scene luminance: %.4f", autoExposure->getLastAvgLuminance());
                            }
                            ImGui::EndDisabled();
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
                    ImGui::EndTabItem();
                }

                // ── Lighting ─────────────────────────────────────────────
                if (ImGui::BeginTabItem("Lighting"))
                {
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
                            glm::vec3 directionalLightDir     = lighting->getDirectionalLightDirection();
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
                                glm::vec3 pointLightPosition  = lighting->getPointLightPosition(i);
                                glm::vec3 pointLightAmbient   = lighting->getPointLightAmbient(i);
                                glm::vec3 pointLightDiffuse   = lighting->getPointLightDiffuse(i);
                                glm::vec3 pointLightSpecular  = lighting->getPointLightSpecular(i);
                                float pointLightConstant      = lighting->getPointLightConstant(i);
                                float pointLightLinear        = lighting->getPointLightLinear(i);
                                float pointLightQuadratic     = lighting->getPointLightQuadratic(i);
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
                            if (ImGui::SliderFloat("Flashlight Cutoff", &flashlightCutoff, 1.0f, 90.0f))
                                lighting->setFlashlightCutoffAngle(flashlightCutoff);
                            if (ImGui::SliderFloat("Flashlight Outer Cutoff", &flashlightOuterCutoff, 1.0f, 90.0f))
                                lighting->setFlashlightOuterCutoffAngle(flashlightOuterCutoff);
                            ImGui::EndTabItem();
                        }
                    }
                    ImGui::EndTabBar();
                    ImGui::EndTabItem();
                }

                // ── Sky & Water ───────────────────────────────────────────
                if (ImGui::BeginTabItem("Sky & Water"))
                {
                    bool skyTimePaused = lighting->isSkyTimePaused();
                    int skyMode = static_cast<int>(lighting->getSkyMode());
                    float skyTimeSpeed = lighting->getSkyTimeSpeed();
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
                            {
                                bool modeChanged = ImGui::RadioButton("Skyrim (pause/step)", &skyMode, 0);
                                ImGui::SameLine();
                                modeChanged |= ImGui::RadioButton("Smooth (linear)", &skyMode, 1);
                                if (modeChanged) {
                                    lighting->setSkyMode(static_cast<uint8_t>(skyMode));
                                    NetSkyTime pkt;
                                    pkt.skyTimeOffset = skyTimeOffset; pkt.sunYawDeg = sunYawDeg;
                                    pkt.skyTimePaused = skyTimePaused; pkt.skyMode = static_cast<uint8_t>(skyMode);
                                    pkt.skyTimeSpeed  = skyTimeSpeed;
                                    pkt.sunStepping   = lighting->getSunStepping();
                                    pkt.sunPauseTimer = lighting->getSunPauseTimer();
                                    pkt.sunStepTimer  = lighting->getSunStepTimer();
                                    udpClient->sendPacket(pkt);
                                }
                            }
                            if (ImGui::SliderFloat("Time Speed", &skyTimeSpeed, 0.001f, 10.0f, "%.3f", ImGuiSliderFlags_Logarithmic)) {
                                lighting->setSkyTimeSpeed(skyTimeSpeed);
                                NetSkyTime pkt;
                                pkt.skyTimeOffset = skyTimeOffset; pkt.sunYawDeg = sunYawDeg;
                                pkt.skyTimePaused = skyTimePaused; pkt.skyMode = static_cast<uint8_t>(skyMode);
                                pkt.skyTimeSpeed  = skyTimeSpeed;
                                pkt.sunStepping   = lighting->getSunStepping();
                                pkt.sunPauseTimer = lighting->getSunPauseTimer();
                                pkt.sunStepTimer  = lighting->getSunStepTimer();
                                udpClient->sendPacket(pkt);
                            }

                            ImGui::Separator();
                            if (ImGui::Checkbox("Pause Sun Animation", &skyTimePaused)) {
                                lighting->setSkyTimePaused(skyTimePaused);
                                NetSkyTime pkt;
                                pkt.skyTimeOffset = skyTimeOffset; pkt.sunYawDeg = sunYawDeg;
                                pkt.skyTimePaused = skyTimePaused; pkt.skyMode = static_cast<uint8_t>(skyMode);
                                pkt.skyTimeSpeed  = skyTimeSpeed;
                                pkt.sunStepping   = lighting->getSunStepping();
                                pkt.sunPauseTimer = lighting->getSunPauseTimer();
                                pkt.sunStepTimer  = lighting->getSunStepTimer();
                                udpClient->sendPacket(pkt);
                            }
                            if (ImGui::SliderFloat("Sun Time Offset (s)", &skyTimeOffset, 0.0f, 60.0f, "%.1f")) {
                                lighting->setSkyTimeOffset(skyTimeOffset);
                                NetSkyTime pkt;
                                pkt.skyTimeOffset = skyTimeOffset; pkt.sunYawDeg = sunYawDeg;
                                pkt.skyTimePaused = skyTimePaused; pkt.skyMode = static_cast<uint8_t>(skyMode);
                                pkt.skyTimeSpeed  = skyTimeSpeed;
                                pkt.sunStepping = false; pkt.sunPauseTimer = 0.0f; pkt.sunStepTimer = 0.0f;
                                udpClient->sendPacket(pkt);
                            }
                            if (ImGui::SliderFloat("Sun Yaw (degrees)", &sunYawDeg, 0.0f, 360.0f, "%.1f")) {
                                lighting->setSunYawDeg(sunYawDeg);
                                NetSkyTime pkt;
                                pkt.skyTimeOffset = skyTimeOffset; pkt.sunYawDeg = sunYawDeg;
                                pkt.skyTimePaused = skyTimePaused; pkt.skyMode = static_cast<uint8_t>(skyMode);
                                pkt.skyTimeSpeed  = skyTimeSpeed;
                                pkt.sunStepping   = lighting->getSunStepping();
                                pkt.sunPauseTimer = lighting->getSunPauseTimer();
                                pkt.sunStepTimer  = lighting->getSunStepTimer();
                                udpClient->sendPacket(pkt);
                            }
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
                        
                        if (ImGui::BeginTabItem("Fog")) {
                            ImGui::Text("Distance Fog");
                            ImGui::Checkbox("Fog Enabled", &fogEnabled);
                            if (fogEnabled) {
                                ImGui::SliderFloat("Fog Start (fraction of chunk radius)", &fogStartFraction, 0.0f, 0.95f, "%.2f");
                                ImGui::SliderFloat("Fog Strength", &fogStrength, 0.1f, 10.0f, "%.1f");
                                ImGui::Text("Fog range: %.0f - %.0f blocks", renderer->getMaxRenderedChunkDist() * fogStartFraction, renderer->getMaxRenderedChunkDist());
                            }
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
                        if (ImGui::BeginTabItem("Water"))
                        {
                            ImGui::SliderFloat("Water wave strength", &waterRenderer->waveStrength, 0.000f, 0.09f, "%.3f");
                            ImGui::SliderFloat("Water dudv tiling",   &waterRenderer->dudvTiling,   0.000f, 0.09f, "%.2f");
                            
                            glm::vec3 underwaterTint = lighting->getUnderwaterTintColor();
                            if (ImGui::ColorEdit3("Underwater Tint", &underwaterTint.x))
                                lighting->setUnderwaterTintColor(underwaterTint);
                            
                            glm::vec3 underwaterFogColor = lighting->getUnderwaterFogColor();
                            if (ImGui::ColorEdit3("Underwater Fog Color", &underwaterFogColor.x))
                                lighting->setUnderwaterFogColor(underwaterFogColor);
                            
                            float underwaterFogDensity = lighting->getUnderwaterFogDensity();
                            if (ImGui::SliderFloat("Underwater Fog Density", &underwaterFogDensity, 0.00f, 0.5f, "%.2f"))
                                lighting->setUnderwaterFogDensity(underwaterFogDensity);
                            ImGui::EndTabItem();
                        }
                    }
                    ImGui::EndTabBar();
                    ImGui::EndTabItem();
                }

                // ── Settings ─────────────────────────────────────────────
                if (ImGui::BeginTabItem("Settings")) {
                    // Audio sliders are skipped entirely when init() failed and we nulled
                    // the manager — the rest of the Settings tab is unrelated and still useful.
                    if (audio) {
                    float masterVolume = audio->getMasterVolume();
                    float musicVolume = audio->getMusicVolume();
                    float sfxVolume = audio->getSfxVolume();
                    if (ImGui::SliderFloat("Master Volume", &masterVolume, 0.0f, 1.0f, "%.2f"))
                        audio->setMasterVolume(masterVolume);
                    if (ImGui::SliderFloat("Music Volume", &musicVolume, 0.0f, 1.0f, "%.2f"))
                        audio->setMusicVolume(musicVolume);
                    if (ImGui::SliderFloat("SFX Volume", &sfxVolume, 0.0f, 1.0f, "%.2f"))
                        audio->setSfxVolume(sfxVolume);

                    // Per-SoundId multipliers (0..2), grouped so the tab isn't 40 flat sliders.
                    auto soundSliders = [&](const char* groupName, std::initializer_list<SoundId> ids) {
                        if (ImGui::TreeNode(groupName)) {
                            for (SoundId id : ids) {
                                float v = audio->getSfxScale(id);
                                if (ImGui::SliderFloat(AudioManager::sfxName(id), &v, 0.0f, 2.0f, "%.2f"))
                                    audio->setSfxScale(id, v);
                            }
                            ImGui::TreePop();
                        }
                    };
                    if (ImGui::CollapsingHeader("Per-sound volumes")) {
                        soundSliders("Footsteps", {
                            SoundId::Footstep_Grass, SoundId::Footstep_Stone, SoundId::Footstep_Wood,
                            SoundId::Footstep_Sand, SoundId::Footstep_Snow, SoundId::Footstep_Gravel,
                            SoundId::Footstep_Leaves, SoundId::Footstep_Water,
                        });
                        soundSliders("Block break", {
                            SoundId::Break_Stone, SoundId::Break_Wood, SoundId::Break_Dirt,
                            SoundId::Break_Sand, SoundId::Break_Gravel, SoundId::Break_Leaves,
                            SoundId::Break_Snow,
                        });
                        soundSliders("Block place", {
                            SoundId::Place_Stone, SoundId::Place_Wood, SoundId::Place_Dirt,
                            SoundId::Place_Sand, SoundId::Place_Gravel, SoundId::Place_Leaves,
                            SoundId::Place_Snow, SoundId::Block_Pop,
                        });
                        soundSliders("Mobs", {
                            SoundId::Zombie_Idle, SoundId::Zombie_Hurt, SoundId::Zombie_Death,
                            SoundId::Zombie_Step,
                            SoundId::Creeper_Idle, SoundId::Creeper_Hurt, SoundId::Creeper_Death,
                            SoundId::Creeper_Fuse, SoundId::Creeper_Explode,
                        });
                        soundSliders("Player", {
                            SoundId::Player_Jump, SoundId::Player_Splash, SoundId::Player_Swim,
                            SoundId::Player_AttackSwing, SoundId::Player_FallSmall,
                            SoundId::Player_FallBig, SoundId::Player_Hurt,
                        });
                        soundSliders("UI", { SoundId::UI_Click });
                    }
                    } // if (audio)

                    if (ImGui::DragFloat("Dbg window Font Size", &style.FontSizeBase, 0.20f, 5.0f, 100.0f, "%.0f"))
                        style._NextFrameFontSizeBase = style.FontSizeBase;
                    ImGui::Separator();
                    static bool survival = camera->getPlayer()->gamemode == GAMEMODES::SURVIVAL;
                    if (ImGui::Checkbox("Survival", &survival))
                    {
                        NetMessage pkt;
                        pkt.message = !survival ? "/gamemode spectator" : "/gamemode survival";
                        udpClient->sendPacket(pkt);
                    }

                    ImGui::Separator();
                    ImGui::Text("Debug spawn");
                    static int debugSpawnCount = 5;
                    ImGui::SliderInt("Count##spawn", &debugSpawnCount, 1, 50);
                    if (ImGui::Button("Spawn Zombies")) {
                        NetMessage pkt;
                        pkt.message = "/summon zombie " + std::to_string(debugSpawnCount);
                        udpClient->sendPacket(pkt);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Spawn Creepers")) {
                        NetMessage pkt;
                        pkt.message = "/summon creeper " + std::to_string(debugSpawnCount);
                        udpClient->sendPacket(pkt);
                    }
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

            ImGui::End();
            if (!uiInteractive) {
                ImGui::PopStyleVar();
            }
        }

        // ── Terrain Debug Window ──
        if (terrainDebugWindow && showDebugWindow) {
            // Get the actual terrain params from the world (server side)
            // For now, use default params but they should persist across frames
            if (!terrainDebugWindowParams) {
                terrainDebugWindowParams = std::make_unique<TerrainGenerationParams>();
            }
            terrainDebugWindow->render(*terrainDebugWindowParams);
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
                    measuredAverageMsDrawShadows + measuredAverageMsGBuffer + measuredAverageMsSSAO +
                    measuredAverageMsDrawWaterReflection + measuredAverageMsDrawWaterRefraction +
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
                showTimingBar("GBuffer",       measuredAverageMsGBuffer,             ImVec4(0.5f, 0.3f, 0.7f, 1.0f));
                showTimingBar("Water Reflect", measuredAverageMsDrawWaterReflection, ImVec4(0.3f, 0.5f, 0.9f, 1.0f));
                showTimingBar("Water Refract", measuredAverageMsDrawWaterRefraction, ImVec4(0.2f, 0.4f, 0.75f, 1.0f));
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
                        measuredAverageMsDrawWaterRefraction = 0.0;
                        measuredAverageMsGBuffer = 0.0;
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
    init(serverIp);
    loadResources();
    render();
}

bool App::connectToServer(const std::string& ip) {
	try {
		udpClient = std::make_unique<UDPClient>(ip.c_str());
	} catch (const std::exception& e) {
		std::cerr << "[Network] Failed to connect: " << e.what() << std::endl;
		udpClient.reset();
		if (multiplayerMenu)
			multiplayerMenu->setErrorMessage(std::string("Could not connect: ") + e.what());
		return false;
	}

	udpClient->sendConnect(settingsMenu->getUsername());

	serverIp = ip;
	setUdpClientPacketCallback();
	return true;
}

void App::transitionTo(GameState newState) {
	gameState = newState;
	switch (newState) {
		case GameState::MainMenu:
			menuManager = mainMenu;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			firstMouse = true;
			break;
		case GameState::Multiplayer:
			menuManager = multiplayerMenu;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;
		case GameState::Settings:
			menuManager = settingsMenu;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;
		case GameState::Controls:
			menuManager = controlsMenu;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			break;
		case GameState::Playing:
			menuManager.reset();
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			firstMouse = true;
			break;
	}
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
    glDeleteQueries(QUERY_POOL_SIZE, queryDrawWaterRefractionPool);
    glDeleteQueries(QUERY_POOL_SIZE, queryGBufferPool);
    glDeleteQueries(QUERY_POOL_SIZE, queryRenderWaterPool);
    glDeleteQueries(QUERY_POOL_SIZE, queryRenderShaderPool);
    glDeleteQueries(QUERY_POOL_SIZE, queryDrawShadowsPool);

	if (udpClient && clientConnected) {
		NetDisconnect pkt;
		pkt.username = settingsMenu ? settingsMenu->getUsername() : "";
		udpClient->sendPacket(pkt);
	}

	// Release GL resources owned via menus (and the shared dirt texture) while
	// the GL context is still current
	mainMenu.reset();
	multiplayerMenu.reset();
	settingsMenu.reset();
	controlsMenu.reset();
    pauseMenu.reset();
    autoExposure.reset();
    sceneFBO.reset();
	if (menuDirtTex) {
		glDeleteTextures(1, &menuDirtTex);
		menuDirtTex = 0;
	}

    if (audio) {
        audio->shutdown();
        audio.reset();
    }

    glfwTerminate();
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

	if (activeHotbarSlot != (uint8_t)-1) camera->getPlayer()->inventory->activeHotbarSlot = activeHotbarSlot;

	inputs.keys = keys;
	inputs.pitch = camera->getPlayer()->getPitch();
	inputs.yaw = camera->getPlayer()->getYaw();
	inputs.loadRadius = camera->getPlayer()->getLoadRadius();
	inputs.activeHotbarSlot = activeHotbarSlot;
	inputs.serverClientReconciliationTick = clientTick;
	// Broadcast our local flashlight state so the server can relay it to other
	// clients via NetEntityMove::positionFlags bit 0x40.
	inputs.playerFlags = lighting && lighting->isFlashlightOn() ? 0x01u : 0u;

	return inputs;
}

void App::processInputMenus(int key, int action) {

	// Handle input for non-Playing menu states
	if (gameState == GameState::Multiplayer) {
		if (key == GLFW_KEY_BACKSPACE && (action == GLFW_PRESS || action == GLFW_REPEAT))
			multiplayerMenu->removeChar();
		else if (key == GLFW_KEY_ENTER && action == GLFW_PRESS) {
			if (multiplayerMenu->getIpAddress().empty()) {
				multiplayerMenu->setErrorMessage("Please enter a server address.");
				return;
			}
			if (connectPending) return;
			if (!connectToServer(multiplayerMenu->getIpAddress()))
				return;
			multiplayerMenu->setErrorMessage("Connecting...");
			connectPending = true;
			connectStartTime = static_cast<float>(glfwGetTime());
		} else if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
			transitionTo(GameState::MainMenu);

		return;
	}
	else if (gameState == GameState::Settings)
	{
		if (key == GLFW_KEY_BACKSPACE && (action == GLFW_PRESS || action == GLFW_REPEAT))
			settingsMenu->removeChar();
		else if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
			transitionTo(GameState::MainMenu);
	}
	else if (gameState == GameState::Controls)
	{
		//just go back to settings menu on escape for now. TODO : make a proper controls menu and handle input there.
		if (controlsMenu->changeControl(key))
			controlsArray = controlsMenu->getControlsArray();
		else if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
			transitionTo(GameState::Settings);
	}

	if (gameState != GameState::Playing) return;

	// Playing state menu handling below
	auto manager = menuManager.lock();

	// HANDLE EVENTS WHEN CHAT OPEN

	if (manager && key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
	{
		menuManager.reset();
		if (!uiInteractive)
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
	}
	//close inventory with E too.
	if (manager && manager == inventoryUI && key == controlsArray[TOGGLE_INVENTORY] && action == GLFW_PRESS)
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
			if (udpClient) udpClient->sendPacket(pkt);

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
		else if (key == controlsArray[CLOSE_WINDOW] && action == GLFW_PRESS)
		{
			menuManager = pauseMenu;
			glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
		}
		else if (key == controlsArray[TOGGLE_INVENTORY] && action == GLFW_PRESS)
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
    static bool f3Held  = false;
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

    // Toggle debug HUD (F3)
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_PRESS && !f3Held &&
        glfwGetKey(window, GLFW_KEY_A) != GLFW_PRESS) {
        showHUD = !showHUD;
        f3Held = true;
    }
    if (glfwGetKey(window, GLFW_KEY_F3) == GLFW_RELEASE) {
        f3Held = false;
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

	// Player list overlay: show while key is held
	playerListVisible = (glfwGetKey(window, controlsArray[PLAYER_LIST]) == GLFW_PRESS);

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
            f1Held = true;
        }
        if (glfwGetKey(window, controlsArray[TOGGLE_WIREFRAME]) == GLFW_RELEASE) {
            f1Held = false;
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
