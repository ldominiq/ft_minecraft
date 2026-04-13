#ifndef TERRAIN_DEBUG_WINDOW_HPP
#define TERRAIN_DEBUG_WINDOW_HPP

#include <vector>
#include <functional>
#include <span>
#include <glad/glad.h>
#include <imgui.h>
#include "Protocol.hpp"
#include "Noise.hpp"

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
    void reseedNoise(int32_t seed);
    void resizeTexture(int newSize);

    // Terrain sampling helpers — mirrors ChunkGeneration private methods exactly
    float sampleContinentalness(const TerrainGenerationParams& p, float wx, float wz);
    float sampleErosion(const TerrainGenerationParams& p, float wx, float wz);
    float samplePV(const TerrainGenerationParams& p, float wx, float wz);
    float interpolateSpline(float t, std::span<const std::pair<float,float>> spline);
    // Returns terrain surface height; writes continentalness, riverMask, lakeMask into out params
    float computeHeight(const TerrainGenerationParams& p, float wx, float wz,
                        float& outCont, float& outRiver, float& outLake);

    GLuint previewTextureID;
    int texSize;

    // Only update texture when a parameter actually changed
    bool dirty;

    void (*regenerateCallback)(void*);
    void* regenerateUserData;

    std::function<void(const NetTerrainParams&)> sendParamsCallback;

    // Debounce timer for sending updates (in milliseconds)
    float sendParamsTimer = 0.0f;
    static constexpr float SEND_PARAMS_DEBOUNCE = 500.0f;

    // ---- View / camera state ----
    int camX = 0;              // world-block X at center of view
    int camZ = 0;              // world-block Z at center of view
    int viewRangeChunks = 200; // total chunks visible across the texture width
    int displaySize = 512;     // pixel size to display the texture (independent of texSize)
    bool highRes = false;      // when true, texSize = 512 (slower but more detail)

    // ---- Noise objects seeded to match ChunkGeneration static locals ----
    Noise noiseBase;        // seed + 0      (continentalness)
    Noise noiseErosion;     // seed + 237
    Noise noisePV;          // seed + 98789
    Noise noiseRiver;       // seed + 7717
    Noise noiseRiverWarpX;  // seed + 7718
    Noise noiseRiverWarpZ;  // seed + 7719
    Noise noiseLake;        // seed + 9901

    int32_t cachedSeed = -999999; // detect seed changes to trigger reseed
};

#endif // TERRAIN_DEBUG_WINDOW_HPP
