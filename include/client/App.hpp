//
// Created by lucas on 6/25/25.
//

#ifndef APP_HPP
#define APP_HPP

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "Camera.hpp"
#include "ChunkRenderer.hpp"
#include "Skybox.hpp"
#include "Shader.hpp"
#include "stb_image.h"
#include "Renderer.hpp"
#include "UDPClient.hpp"

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

    void cleanup();
    void setUdpClientPacketCallback();
	NetPlayerInputs buildPlayerInputsPacket();
    void processInput();
    void updateWindowTitle();
    void toggleDisplayMode();

    void loadControlsDefaults();
	void saveControls(const char* filename = "controls.cfg");
	void loadControlsFromFile(const char* filename = "controls.cfg");

    void debugWindow();

    GLFWwindow* window;

	uint16_t inputMask = 0;
    bool keyPressedRecently = false;

    unsigned int VAO, VBO, EBO, shaderProgram, texture, lightCubeVAO, lightCubeVBO;
    unsigned int depthMapFBO, depthMap;
    const unsigned int SHADOW_WIDTH = 1024, SHADOW_HEIGHT = 1024;
    unsigned int quadVAO = 0;
    unsigned int quadVBO;
    unsigned int planeVAO;

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

    std::unique_ptr<Skybox> skybox;
    std::shared_ptr<Shader> textureShader;
    std::shared_ptr<Shader> gradientShader;
    std::shared_ptr<Shader> skyShader;
    std::shared_ptr<Shader> lightCubeShader;
    std::shared_ptr<Shader> activeShader;   // pointer to the currently active shader program
    std::shared_ptr<Shader> simpleDepthShader;
    std::shared_ptr<Shader> debugDepthQuad;

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

    // Lighting parameters that can be tweaked via ImGui.  The direction
    // should be normalised each frame; colours are in [0,1].
    
    
    // Directional light (sun)
    bool directionalLightOn = true;
    glm::vec3 directionalLightDir  = glm::vec3(0.5f, 1.0f, 0.3f);
    glm::vec3 directionalAmbientColor = glm::vec3(0.3f);
    glm::vec3 directionalDiffuseColor = glm::vec3(1.0f);
    glm::vec3 directionalSpecularColor = glm::vec3(1.0f);

    // Point light (lamp)
    std::vector<bool> pointLightsOn = {true, true, true, true};
    glm::vec3 pointLightPositions[4] = {
        glm::vec3( 0.7f,  86.0f,  2.0f),
        glm::vec3( 2.3f, 85.3f, -4.0f),
        glm::vec3(-4.0f,  84.0f, -12.0f),
        glm::vec3( 0.0f,  83.0f, -3.0f)
    };
    glm::vec3 pointLightAmbient[4] = {
        glm::vec3(0.05f),
        glm::vec3(0.05f),
        glm::vec3(0.05f),
        glm::vec3(0.05f)
    };
    glm::vec3 pointLightDiffuse[4] = {
        glm::vec3(0.8f),
        glm::vec3(0.8f),
        glm::vec3(0.8f),
        glm::vec3(0.8f)
    };
    glm::vec3 pointLightSpecular[4] = {
        glm::vec3(1.0f),
        glm::vec3(1.0f),
        glm::vec3(1.0f),
        glm::vec3(1.0f)
    };
    float pointLightConstant[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    float pointLightLinear[4] = { 0.09f, 0.09f, 0.09f, 0.09f };
    float pointLightQuadratic[4] = { 0.032f, 0.032f, 0.032f, 0.032f };

    // Flashlight
    bool flashlightOn = true;
    float spotLightConstant = 1.0f;
    float spotLightLinear = 0.09f;
    float spotLightQuadratic = 0.032f;
    float flashlightCutoff = 12.5f; // spotlight cutoff angle in degrees
    float flashlightOuterCutoff = 17.5f; // spotlight outer cutoff angle in degrees

    float materialShininess = 32.0f; // material shininess factor


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

    // --- Sky controls ---
    // Control sun position over time
    float skyTimeOffset = 0.0f;
    bool skyTimePaused = false;
    // Simple tone-mapping exposure for sky shader
    float skyExposure = 1.2f;
    // Atmospheric density and thickness scalars (1.0 ~ Earth-like)
    float skyAtmDensity = 19.0f;
    float skyAtmThickness = 1.0f;
    float planetScale = 7900.0f;
};

#endif //APP_HPP