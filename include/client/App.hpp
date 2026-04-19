//
// Created by lucas on 6/25/25.
//

#ifndef APP_HPP
#define APP_HPP

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#endif

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Camera.hpp"
#include "ChunkRenderer.hpp"
#include "Lighting.hpp"
#include "Shader.hpp"
#include "Renderer.hpp"
#include "UDPClient.hpp"
#include "Chat.hpp"
#include "ItemPropEntityManager.hpp"
#include "WaterFramebuffer.hpp"
#include "RenderTypeFramebuffer.hpp"
#include "WaterRenderer.hpp"
#include "GuiTexture.hpp"
#include "InventoryUI.hpp"
#include "GBuffer.hpp"
#include "SSAO.hpp"
#include "TextureManager.hpp"
#include "ui/TerrainDebugWindow.hpp"
#include "FogUniforms.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
// glm for vector types used in lighting controls
#include <glm/vec3.hpp>
#include <cstdint>
#include <memory>
#include <algorithm>

#include <optional>
#include <unordered_map>

#include <thread>
#include <chrono>

// ImGui includes
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#if !defined(_WIN32)
#include <unistd.h> // for sysconf
#else
#include <windows.h>
#include <psapi.h>
#endif
#include <stdio.h>  // for FILE, fopen
#include <cstdlib>

#include "GuiRenderer.hpp"
#include "ChunkBoundaryRenderer.hpp"
#include "DebugHUD.hpp"
#include "PlayerListHUD.hpp"
#include "MainMenu.hpp"
#include "MultiplayerMenu.hpp"
#include "SettingsMenu.hpp"

#define CONTROL_LIST 		\
    X(FORWARD)       		\
    X(BACKWARD)      		\
    X(LEFT)          		\
    X(RIGHT)         		\
    X(UP)            		\
    X(DOWN)             	\
    X(MOVE_FAST)        	\
    X(LEFT_CLICK)       	\
    X(TOGGLE_FULLSCREEN)	\
    X(TOGGLE_WIREFRAME)		\
    X(TOGGLE_SHADER)		\
    X(TOGGLE_DEBUG)			\
    X(CLOSE_WINDOW)			\
	X(THIRD_PERSON_CAMERA)	\
	X(PLAYER_LIST)			\
							\
	X(HOTBAR_1)				\
	X(HOTBAR_2)				\
	X(HOTBAR_3)				\
	X(HOTBAR_4)				\
	X(HOTBAR_5)				\
	X(HOTBAR_6)				\
	X(HOTBAR_7)				\
	X(HOTBAR_8)				\
	X(HOTBAR_9)				\

enum controls {
#define X(name) name,
    CONTROL_LIST
#undef X
    CONTROL_COUNT
};

// Read a GPU timer query result and apply exponential moving average.
// Returns true if a new sample was read, false if query wasn't ready.
bool readGPUQueryEMA(GLuint queryId, double &smoothedMs, float alpha);

class App {
public:
    enum class GameState {
        MainMenu,
        Multiplayer,
        Settings,
        Playing
    };

    struct TerrainDebugUIParams {
        int genSize = 1000;
        int downsample = 16;
    };

    App(const std::string& serverIp = "127.0.0.1");
    ~App();

    void run();

    GameState gameState = GameState::MainMenu;

private:
    void init(const std::string& serverIp);
    void loadResources();
    void render();
	void renderScene(const glm::mat4 &view, const glm::mat4 &projection, glm::vec4 clipPlane) const;
	void gameTick();

    void cleanup();
    void setUdpClientPacketCallback();
	NetPlayerInputs buildPlayerInputsPacket();
    void processInput();
	void processInputMenus(int key, int action);
	bool connectToServer(const std::string& ip);
	void transitionTo(GameState newState);
    void updateWindowTitle();
    void toggleDisplayMode();

    void loadControlsDefaults();
	void saveControls(const char* filename = "controls.cfg");
	void loadControlsFromFile(const char* filename = "controls.cfg");

    void debugWindow();
    void computeDebugStats();



    GLFWwindow* window;

    bool vsync = true;

	uint16_t inputMask = 0;
    bool keyPressedRecently = false;
	bool mouseMovedRecently = false;
	float lastMouseMoveTime = 0;
	float glfwTickTime = 0;

    TextureManager textureManager;

    enum class DisplayMode {
        Windowed,
        Fullscreen
    };
    DisplayMode displayMode = DisplayMode::Fullscreen;

    std::shared_ptr<Camera> camera;
	GLFWmonitor* monitor;
    const GLFWvidmode* mode;

	std::unique_ptr<ItemPropEntityManager> m_itemPropEntityManager;

	std::shared_ptr<Renderer> renderer;
	std::unique_ptr<WaterRenderer> waterRenderer;
	std::unique_ptr<ChunkBoundaryRenderer> chunkBoundaryRenderer;
	std::unique_ptr<UDPClient> udpClient;

    std::shared_ptr<Lighting> lighting;
    std::shared_ptr<Shader> textureShader;
    std::shared_ptr<Shader> gradientShader;

    std::shared_ptr<Shader> activeShader;   // pointer to the currently active shader program

	//menus
	std::weak_ptr<Menu> menuManager;
	std::shared_ptr<Chat> chat;
	std::shared_ptr<InventoryUI> inventoryUI;
	std::unique_ptr<DebugHUD> debugHUD;
	bool showHUD = false;

	// Main menu screens
	std::shared_ptr<MainMenu> mainMenu;
	std::shared_ptr<MultiplayerMenu> multiplayerMenu;
	std::shared_ptr<SettingsMenu> settingsMenu;

	std::unique_ptr<PlayerListHUD> playerListHUD;
	bool     playerListVisible = false;
	uint32_t localClientId     = 0;
	std::unordered_map<uint32_t, float> remotePings; // entityId -> pingMs
	DebugStats cachedDebugStats{};

	std::shared_ptr<Loader> loader;
	// GUI
	std::vector<GuiTexture> guis;
	std::unique_ptr<GuiRenderer> guiRenderer;


	// Water
	std::shared_ptr<WaterFramebuffer> waterFramebuffer;
	std::shared_ptr<Shader> waterShader;
    GLuint dudvTexture, waterNormalTexture;

	// Render type debug framebuffers
	std::unique_ptr<RenderTypeFramebuffer> renderTypeFramebuffer;

    // Terrain parameter debugging and tweaking
    std::unique_ptr<TerrainDebugWindow> terrainDebugWindow;
    std::unique_ptr<TerrainGenerationParams> terrainDebugWindowParams;
    // SSAO
    std::shared_ptr<GBuffer> gBuffer;
    std::shared_ptr<SSAO> ssao;
    std::shared_ptr<Shader> gBufferShader;

	std::optional<int> seed;

    uint8_t currentBiome = 0;
    int currentTerrainHeight = 0;
    int currentSeaLevel = 64;
    int currentWorldSeed = 0;
    float currentContinentalness = 0.0f;
    float currentErosion = 0.0f;
    float currentPeakValley = 0.0f;
    float currentTemperature = 0.0f;
    float currentHumidity = 0.0f;
    uint8_t currentContBucket = 0;
    uint8_t currentErosionBucket = 0;
    uint8_t currentPVBucket = 0;
    uint8_t currentTempBucket = 0;
    uint8_t currentHumidBucket = 0;

    TerrainDebugUIParams debugTerrainParams;

    float lastX = 400, lastY = 300;
    bool firstMouse = true;
    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

	bool clientConnected = false;

    bool wireframe = false;

    float lastTitleUpdate = 0.0f;
    int frameCount = 0;

    int windowedX = 100;
    int windowedY = 100;
    int windowedWidth = 1280;
    int windowedHeight = 720;
	int screenWidth = 1280;
	int screenHeight = 720;

    std::string serverIp;

    bool useGradientShader = false;

    float renderDistance = 1000.0f; // Distance of the far clipping plane

    // Distance fog
    float fogStartFraction = 0.5f;  // fog begins at this fraction of maxRenderedChunkDist (0=camera, 1=edge) (the nearest unloaded chunk boundary)
    float fogStrength = 1.4f;       // exponent on fog ramp: 1=linear, 10=at the edge
    bool  fogEnabled = true;

    // Variables for smoothing the FPS shown in the debug UI.  We maintain a
    // moving average of frame times over a sample buffer to reduce jitter.
    std::vector<float> fpsSamples;
    static const size_t fpsSampleCount = 60;
    float uiDisplayFPS = 0.0f;

    // Helper to query the current process’s resident set size (RSS) in bytes.
    // Used in the debug UI to show approximate memory usage.
    static size_t getCurrentRSS();

    // --- ImGui / UI state ---
    // Whether the debug overlay is interactive.  When true the mouse is
    // released and the debug window captures input; when false the window
    // remains visible but does not capture input.
    bool uiInteractive = false;
    // Internal flag to handle key debounce for toggling the interactive mode.
    bool uiToggleHeld = false;
	bool showDebugWindow = true;

	//TODO: put in struct
	// Debug framebuffer view toggles
	bool showReflectionTexture = false;
	bool showRefractionTexture = false;
	bool showRefractionDepthTexture = false;
	bool showNormalsTexture = false;
	bool showDepthTexture = false;
	bool showSSAOTexture = false;
	bool showSSAORawTexture = false;
	bool showGBufferPositionTexture = false;
	bool showGBufferNormalTexture = false;
	bool showFrustumCullingDebug = false;

    int selectedRenderType = 0; // 0 = none, 1 = normals, 2 = depth

	//keeps track of control GLFW values
    int controlsArray[CONTROL_COUNT];
	//keeps track of control names so they can be inserted/read from the .config file
	const char* controlNames[CONTROL_COUNT] = {
	#define X(name) #name,
		CONTROL_LIST
	#undef X
	};

    // PROFILING
    bool profilingEnabled = false;
    bool showProfilerWindow = false;
    static constexpr int QUERY_POOL_SIZE = 4; // 3+ frames of latency to avoid reading before GPU is done
    GLuint queryDrawSkyPool[QUERY_POOL_SIZE]{};
    GLuint queryDrawCloudsPool[QUERY_POOL_SIZE]{};
    GLuint queryDrawWaterReflectionPool[QUERY_POOL_SIZE]{};
    GLuint queryDrawWaterRefractionPool[QUERY_POOL_SIZE]{};
    GLuint queryDrawShadowsPool[QUERY_POOL_SIZE]{};
    GLuint queryRenderShaderPool[QUERY_POOL_SIZE]{};
    GLuint queryRenderWaterPool[QUERY_POOL_SIZE]{};
    GLuint queryDrawEntities[QUERY_POOL_SIZE]{};
    GLuint querySSAOPool[QUERY_POOL_SIZE]{};
    GLuint queryGBufferPool[QUERY_POOL_SIZE]{};

    // Track which queries were actually issued this frame (conditional passes like shadows/SSAO)
    bool shadowQueryIssuedThisFrame[QUERY_POOL_SIZE]{};
    bool ssaoQueryIssuedThisFrame[QUERY_POOL_SIZE]{};

    int currentQueryIndex = 0;

    // EMA smoothing factor: 0.05 = slow/smooth, 0.3 = fast/responsive
    float profilingEMASmoothing = 0.1f;

    double measuredAverageMsDrawSky = 0.0;
    double measuredAverageMsDrawClouds = 0.0;
    double measuredAverageMsDrawWaterReflection = 0.0;
    double measuredAverageMsDrawWaterRefraction = 0.0;
    double measuredAverageMsDrawShadows = 0.0;
    double measuredAverageMsRenderShader = 0.0;
    double measuredAverageMsRenderWater = 0.0;
    double measuredAverageMsDrawEntities = 0.0;
    double measuredAverageMsSSAO = 0.0;
    double measuredAverageMsGBuffer = 0.0;

	// Ping measurement
    float pingMs = -1.0f;
    float pingEMASmoothing = 1.0f;
    float lastPingSentTime = -999.0f;
	uint64_t lastPingTimestamp = 0;
};

#endif //APP_HPP