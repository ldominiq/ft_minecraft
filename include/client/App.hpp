//
// Created by lucas on 6/25/25.
//

#ifndef APP_HPP
#define APP_HPP

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Camera.hpp"
#include "ChunkRenderer.hpp"
#include "Lighting.hpp"
#include "Shader.hpp"
#include "stb_image.h"
#include "Renderer.hpp"
#include "UDPClient.hpp"
#include "Chat.hpp"
#include "WaterFramebuffer.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
// glm for vector types used in lighting controls
#include <glm/vec3.hpp>
#include <memory>

#include <optional>

#include <thread>
#include <chrono>

// ImGui includes
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <unistd.h> // for sysconf
#include <stdio.h>  // for FILE, fopen
#include <cstdlib>

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

enum controls {
#define X(name) name,
    CONTROL_LIST
#undef X
    CONTROL_COUNT
};


class App {
public:
    App();
    ~App();

    void run();

private:
    void init();
    void loadResources();
    static unsigned int loadTexture(const char* path);
    void render();
	void gameTick();

    void cleanup();
    void setUdpClientPacketCallback();
	NetPlayerInputs buildPlayerInputsPacket();
    void processInput();
	void processInputsMenus(int key, int action);
    void updateWindowTitle();
    void toggleDisplayMode();

    void loadControlsDefaults();
	void saveControls(const char* filename = "controls.cfg");
	void loadControlsFromFile(const char* filename = "controls.cfg");

    void debugWindow();

    GLFWwindow* window;

    bool vsync = true;

	uint16_t inputMask = 0;
    bool keyPressedRecently = false;
	bool mouseMovedRecently = false;
	float lastMouseMoveTime = 0;
	float lastTickClientTime = 0;

    unsigned int texture;

    enum class DisplayMode {
        Windowed,
        Fullscreen
    };
    DisplayMode displayMode = DisplayMode::Fullscreen;

    std::unique_ptr<Camera> camera;
	GLFWmonitor* monitor;
    const GLFWvidmode* mode;

	std::unique_ptr<Renderer> renderer;
	std::unique_ptr<UDPClient> udpClient;

    std::unique_ptr<Lighting> lighting;
    std::shared_ptr<Shader> textureShader;
    std::shared_ptr<Shader> gradientShader;
    
    std::shared_ptr<Shader> activeShader;   // pointer to the currently active shader program

	//menus
	std::shared_ptr<Menu> menuManager;
	std::shared_ptr<Chat> chat;

    std::unique_ptr<WaterFramebuffer> waterFBO;
    std::shared_ptr<Shader> waterShader;
    GLuint dudvTexture, waterNormalTexture;
    float waterMoveFactor = 0.0f;

	std::optional<int> seed;

    u_int8_t currentBiome;

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

    bool useGradientShader = false;

    float renderDistance = 1000.0f; // Distance of the far clipping plane

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

	//keeps track of control GLFW values
    int controlsArray[CONTROL_COUNT];
	//keeps track of control names so they can be inserted/read from the .config file
	const char* controlNames[CONTROL_COUNT] = {
	#define X(name) #name,
		CONTROL_LIST
	#undef X
	};

    
};

#endif //APP_HPP