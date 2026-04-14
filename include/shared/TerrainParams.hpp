//
// Created by lucas on 8/6/25.
//
// Shared terrain generation parameters - used by both client and server

#ifndef TERRAIN_PARAMS_HPP
#define TERRAIN_PARAMS_HPP

#include <array>
#include <utility>
#include <cstdint>

// Spline control points: {continentalness, height}
inline constexpr std::array<std::pair<float, float>, 9> continentalnessSpline = {{

    // VALUES REQUIRING TINKERING
    {-3.8f, 256.0f }, // Mushroom land
    { -1.0f, 40.0f }, // Ocean floor
    { -0.52f, 45.0f },
    { -0.19f, 72.0f }, // Coast
    { -0.11f, 75.0f }, // Coast
    { 0.03f, 87.0f }, // Near inland
    { 0.3f, 90.0f }, // Mid inland
    { 0.9f, 170.0f }, // 
    { 3.8f, 256.0f }  // Mountains / plateau

}};

inline constexpr std::array<std::pair<float, float>, 8> erosionSpline = {{
    // VALUES REQUIRING TINKERING
    { -1.00f, 256.0f },
    { -0.78f, 210.0f },
    { -0.375f, 170.0f },
    { -0.2225f, 120.0f },
    {  0.05f,  72.0f },
    {  0.45f,  86.0f },
    {  0.55f,  89.0f },
    {  1.00f,  62.0f }

}};

inline constexpr std::array<std::pair<float, float>, 5> peakValleySpline = {{
    // VALUES REQUIRING TINKERING
    { -1.00f, -120.0f }, // Valleys
    { -0.6f, -90.0f },
    { 0.0f, 0.0f }, // Middle
    {  0.7f, 120.0f }, // High
    {  1.0f, 190.0f } // Peaks
}};

// Terrain / generation tunables exposed to code / UI
struct TerrainGenerationParams {
    // deterministic seed
    int32_t seed = 1337;

    // basic levels
    int seaLevel = 64;
    int bedrockLevel = 0;

    // River carving params (server-side, no ImGui required)
    float riverFrequency = 0.008f;
    int riverOctaves = 4;
    float riverPersistence = 0.218f;
    float riverLacunarity = 2.646f;
    float riverWidth = 0.028f;
    float riverBankFeather = 0.1f;
    float riverDepth = 25.276f;
    float riverWarpFrequency = 0.004f;
    float riverWarpStrength = 188.979f;
    float riverMinContinentalness = -0.359f;
    float riverMaxContinentalness = 0.302f;   // blocks rivers on high-continentalness (mountain) terrain

    // Lake carving params (same water level as ocean/sea)
    float lakeFrequency = 0.027f;
    int lakeOctaves = 3;
    float lakePersistence = 0.312f;
    float lakeLacunarity = 2.171f;
    float lakeThreshold = 0.356f;
    float lakeFeather = 0.331f;
    float lakeDepth = 46.290f;
    float lakeMinContinentalness = 0.121f;
    float lakeMaxContinentalness = 0.461f;    // blocks lakes on high-continentalness (mountain) terrain


    // heightmap dump settings / helpers (used by World::dumpHeightmap)
    int genSize = 500;     // default size for quick dumps
    int downsample = 8;    // output downsample factor for dumps

    // Continentalness noise params
    float continentalnessFrequency = 0.001f;
    int continentalnessOctaves = 5;
    float continentalnessPersistence = 0.666f;
    float continentalnessLacunarity = 1.473f;
    float continentalnessScalingFactor = 2.558f;

    // Erosion noise params
    float erosionFrequency = 0.009f;
    int erosionOctaves = 5;
    float erosionPersistence = 0.35f;
    float erosionLacunarity = 2.37f;
    float erosionScalingFactor = 2.0f;

    // Peak / valley noise params
    float peakValleyFrequency = 0.001f;
    int peakValleyOctaves = 5;
    float peakValleyPersistence = 0.271f;
    float peakValleyLacunarity = 1.438f;
    float peakValleyScalingFactor = 2.5f;

    // BIOMES

    // Temperature noise params
    float temperatureFrequency = 0.0012f;
    int temperatureOctaves = 4;
    float temperaturePersistence = 0.50f;
    float temperatureLacunarity = 2.0f;
    float temperatureScalingFactor = 0.5f;


    // Humidity noise params
    float humidityFrequency = 0.0015f;
    int humidityOctaves = 4;
    float humidityPersistence = 0.50f;
    float humidityLacunarity = 2.0f;
    float humidityScalingFactor = 0.5f;

    // Size of biome regions in chunks (larger -> larger contiguous biomes)
    int biomeScaleChunks = 8;
    bool snapClimateToCells = true;
    float climateWarpFrequency = 0.0008f;
    float climateWarpStrength = 180.0f;

    float desertMoistureThreshold = 0.30f;
    float forestMoistureThreshold = 0.60f;
    float snowTemperatureThreshold = 0.28f;

    bool debugOresOnly = false; // if true, strip all other solid blocks to AIR
};

#endif // TERRAIN_PARAMS_HPP
