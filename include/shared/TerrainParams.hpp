//
// Created by lucas on 8/6/25.
//
// Shared terrain generation parameters - used by both client and server

#ifndef TERRAIN_PARAMS_HPP
#define TERRAIN_PARAMS_HPP

#include <vector>
#include <cstdint>

// Spline control points: {continentalness, height}
static const std::vector<std::pair<float, float>> continentalnessSpline = {

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

};

static const std::vector<std::pair<float, float>> erosionSpline = {
    // VALUES REQUIRING TINKERING
    { -1.00f, 256.0f },
    { -0.78f, 210.0f },
    { -0.375f, 170.0f },
    { -0.2225f, 120.0f },
    {  0.05f,  72.0f },
    {  0.45f,  86.0f },
    {  0.55f,  89.0f },
    {  1.00f,  62.0f }

};

static const std::vector<std::pair<float, float>> peakValleySpline = {
    // VALUES REQUIRING TINKERING
    { -1.00f, -120.0f }, // Valleys
    { -0.6f, -90.0f },
    { 0.0f, 0.0f }, // Middle
    {  0.7f, 120.0f }, // High
    {  1.0f, 190.0f } // Peaks
};

// Terrain / generation tunables exposed to code / UI
struct TerrainGenerationParams {
    // deterministic seed
    int32_t seed = 1337;

    // basic levels
    int seaLevel = 64;
    int bedrockLevel = 0;

    // River carving params (server-side, no ImGui required)
    float riverFrequency = 0.0048f;
    int riverOctaves = 4;
    float riverPersistence = 0.5f;
    float riverLacunarity = 2.0f;
    float riverWidth = 0.030f;
    float riverBankFeather = 0.060f;
    float riverDepth = 18.0f;
    float riverWarpFrequency = 0.0012f;
    float riverWarpStrength = 180.0f;
    float riverMinContinentalness = -0.04f;

    // Lake carving params (same water level as ocean/sea)
    float lakeFrequency = 0.0010f;
    int lakeOctaves = 3;
    float lakePersistence = 0.5f;
    float lakeLacunarity = 2.0f;
    float lakeThreshold = 0.62f;
    float lakeFeather = 0.14f;
    float lakeDepth = 10.0f;
    float lakeMinContinentalness = -0.02f;


    // heightmap dump settings / helpers (used by World::dumpHeightmap)
    int genSize = 500;     // default size for quick dumps
    int downsample = 8;    // output downsample factor for dumps

    // Continentalness noise params
    float continentalnessFrequency = 0.001f;
    int continentalnessOctaves = 5;
    float continentalnessPersistence = 0.245f;
    float continentalnessLacunarity = 3.250f;
    float continentalnessScalingFactor = 4.5f;

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
