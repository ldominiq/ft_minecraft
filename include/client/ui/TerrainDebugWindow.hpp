#ifndef TERRAIN_DEBUG_WINDOW_HPP
#define TERRAIN_DEBUG_WINDOW_HPP

#include <vector>
#include <glad/glad.h>
#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include "Protocol.hpp"
#include <functional>

class TerrainDebugWindow {
public:
    TerrainDebugWindow();
    ~TerrainDebugWindow();

    // Call this inside your ImGui block in App.cpp
    void render(TerrainGenerationParams& params);

    // Provide a callback to be fired when the user clicks "Regenerate Terrain"
    void setRegenerateCallback(void(*callback)(void*), void* userData);
    
    // Provide a callback to send terrain params to the server
    void setSendParamsCallback(std::function<void(const NetTerrainParams&)> callback) {
        sendParamsCallback = std::move(callback);
    }

private:
    void updateTexture(const TerrainGenerationParams& params);

    GLuint previewTextureID;
    int texSize;

    // We only update the texture if a parameter actually changed.
    bool dirty;

    void (*regenerateCallback)(void*);
    void* regenerateUserData;
    
    std::function<void(const NetTerrainParams&)> sendParamsCallback;
    
    // Debounce timer for sending updates (in milliseconds)
    float sendParamsTimer = 0.0f;
    static constexpr float SEND_PARAMS_DEBOUNCE = 500.0f; // 500ms debounce
};

#endif // TERRAIN_DEBUG_WINDOW_HPP