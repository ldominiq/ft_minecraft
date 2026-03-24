#include "ui/TerrainDebugWindow.hpp"
#include <iostream>

TerrainDebugWindow::TerrainDebugWindow()
    : texSize(256), dirty(true), regenerateCallback(nullptr), regenerateUserData(nullptr)
{
    glGenTextures(1, &previewTextureID);
    glBindTexture(GL_TEXTURE_2D, previewTextureID);
    
    // Setup generic texture flags
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Initial allocation
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

TerrainDebugWindow::~TerrainDebugWindow() {
    glDeleteTextures(1, &previewTextureID);
}

void TerrainDebugWindow::setRegenerateCallback(void(*callback)(void*), void* userData) {
    regenerateCallback = callback;
    this->regenerateUserData = userData;
}

void TerrainDebugWindow::updateTexture(const TerrainGenerationParams& params) {
    // Generate a simplified 2D preview using Perlin noise to approximate terrain height
    // This shows a basic visualization of terrain height distribution
    std::vector<uint32_t> pixels(texSize * texSize);

    // Improved Perlin-like noise generator
    auto perlin = [](uint32_t seed, float x, float y) -> float {
        uint32_t xi = (uint32_t)x;
        uint32_t yi = (uint32_t)y;
        float xf = x - xi;
        float yf = y - yi;
        
        // Smooth interpolation
        float u = xf * xf * (3.0f - 2.0f * xf);
        float v = yf * yf * (3.0f - 2.0f * yf);
        
        // Hash corners
        auto hash2 = [seed](uint32_t x, uint32_t y) -> float {
            uint32_t h = seed ^ (x * 73856093U) ^ (y * 19349663U);
            h ^= (h >> 13);
            h ^= (h << 17);
            h ^= (h >> 5);
            return 2.0f * (float)(h & 0x7FFFFFFFU) / (float)0x7FFFFFFFU - 1.0f;
        };
        
        float n00 = hash2(xi, yi);
        float n10 = hash2(xi + 1, yi);
        float n01 = hash2(xi, yi + 1);
        float n11 = hash2(xi + 1, yi + 1);
        
        float nx0 = n00 * (1.0f - u) + n10 * u;
        float nx1 = n01 * (1.0f - u) + n11 * u;
        return nx0 * (1.0f - v) + nx1 * v;
    };

    // Generate preview texture
    for (int y = 0; y < texSize; ++y) {
        for (int x = 0; x < texSize; ++x) {
            // Sample terrain using multiple octaves of noise
            float worldX = (float)x * 0.02f;  // Scale for preview
            float worldY = (float)y * 0.02f;
            
            // Combine continentalness and erosion for height
            float height = 0.0f;
            float amplitude = 1.0f;
            float frequency = 1.0f;
            float maxAmplitude = 0.0f;

            // Continentalness octaves
            for (int i = 0; i < params.continentalnessOctaves && i < 6; ++i) {
                float sample = perlin(params.seed + i * 100, 
                                    worldX * frequency * params.continentalnessFrequency * 500.0f,
                                    worldY * frequency * params.continentalnessFrequency * 500.0f);
                height += sample * amplitude;
                maxAmplitude += amplitude;
                amplitude *= params.continentalnessPersistence;
                frequency *= params.continentalnessLacunarity;
            }

            height = (height / maxAmplitude) * 0.5f;  // Scale by continentalness effect
            
            // Add erosion variation
            amplitude = 1.0f;
            frequency = 1.0f;
            maxAmplitude = 0.0f;
            for (int i = 0; i < std::min(params.erosionOctaves, 4) && i < 4; ++i) {
                float sample = perlin(params.seed + 200 + i * 100,
                                    worldX * frequency * params.erosionFrequency * 500.0f,
                                    worldY * frequency * params.erosionFrequency * 500.0f);
                height += sample * amplitude * 0.3f;
                maxAmplitude += amplitude;
                amplitude *= params.erosionPersistence;
                frequency *= params.erosionLacunarity;
            }

            height = std::clamp(height, -2.0f, 2.0f);
            float normalizedHeight = (height + 2.0f) / 4.0f;  // Normalize to [0, 1]
            normalizedHeight = std::clamp(normalizedHeight, 0.0f, 1.0f);

            // Map height to colors: deep water -> shallow water -> sand -> grass -> forest -> mountains -> snow
            uint8_t r = 0, g = 0, b = 0, a = 255;

            if (normalizedHeight < 0.3f) {
                // Deep water - dark blue
                float t = normalizedHeight / 0.3f;
                b = (uint8_t)(100 + t * 100);
                g = (uint8_t)(60 + t * 60);
                r = (uint8_t)(20 + t * 30);
            } else if (normalizedHeight < 0.4f) {
                // Shallow water - bright blue
                float t = (normalizedHeight - 0.3f) / 0.1f;
                b = (uint8_t)(200 - t * 50);
                g = (uint8_t)(120 + t * 30);
                r = (uint8_t)(50 + t * 50);
            } else if (normalizedHeight < 0.45f) {
                // Sand/Beach - tan
                float t = (normalizedHeight - 0.4f) / 0.05f;
                r = (uint8_t)(200 - t * 30);
                g = (uint8_t)(180 - t * 30);
                b = (uint8_t)(100 + t * 20);
            } else if (normalizedHeight < 0.65f) {
                // Grass - green
                float t = (normalizedHeight - 0.45f) / 0.2f;
                r = (uint8_t)(100 - t * 50);
                g = (uint8_t)(180 + t * 40);
                b = (uint8_t)(80 - t * 30);
            } else if (normalizedHeight < 0.8f) {
                // Forest - dark green
                float t = (normalizedHeight - 0.65f) / 0.15f;
                r = (uint8_t)(50 - t * 20);
                g = (uint8_t)(140 - t * 40);
                b = (uint8_t)(50 - t * 20);
            } else if (normalizedHeight < 0.9f) {
                // Mountains - gray/brown
                float t = (normalizedHeight - 0.8f) / 0.1f;
                r = (uint8_t)(120 + t * 60);
                g = (uint8_t)(110 + t * 60);
                b = (uint8_t)(100 + t * 60);
            } else {
                // Snow peaks - white
                float t = (normalizedHeight - 0.9f) / 0.1f;
                r = (uint8_t)(220 + t * 35);
                g = (uint8_t)(220 + t * 35);
                b = (uint8_t)(220 + t * 35);
            }

            pixels[y * texSize + x] = r | (g << 8) | (b << 16) | (a << 24);
        }
    }

    glBindTexture(GL_TEXTURE_2D, previewTextureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texSize, texSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    dirty = false;
}

void TerrainDebugWindow::render(TerrainGenerationParams& params) {
    if (dirty) updateTexture(params);
    
    // Update debounce timer
    sendParamsTimer += ImGui::GetIO().DeltaTime * 1000.0f; // Convert to milliseconds

    ImGui::Begin("Terrain Debugger");

    ImGui::Text("Terrain Height Preview (%dx%d)", texSize, texSize);
    ImGui::TextDisabled("Colors: Blue=Water, Tan=Beach, Green=Grass, Dark=Forest, Gray=Mountains, White=Peaks");
    ImGui::Image((void*)(intptr_t)previewTextureID, ImVec2((float)texSize, (float)texSize));

    ImGui::Separator();
    ImGui::TextDisabled("Note: Regenerate World reloads all chunks with new terrain params.");
    ImGui::TextDisabled("You must click 'Sync to Server' first to send parameters.");
    
    if (ImGui::Button("Regenerate World") && regenerateCallback) {
        regenerateCallback(regenerateUserData);
    }
    
    // Quick sync button
    ImGui::SameLine();
    if (ImGui::Button("Sync to Server") && sendParamsCallback) {
        NetTerrainParams pkt;
        pkt.seed = params.seed;
        pkt.seaLevel = params.seaLevel;
        pkt.bedrockLevel = params.bedrockLevel;
        pkt.riverFrequency = params.riverFrequency;
        pkt.riverOctaves = params.riverOctaves;
        pkt.riverPersistence = params.riverPersistence;
        pkt.riverLacunarity = params.riverLacunarity;
        pkt.riverWidth = params.riverWidth;
        pkt.riverBankFeather = params.riverBankFeather;
        pkt.riverDepth = params.riverDepth;
        pkt.riverWarpFrequency = params.riverWarpFrequency;
        pkt.riverWarpStrength = params.riverWarpStrength;
        pkt.riverMinContinentalness = params.riverMinContinentalness;
        pkt.lakeFrequency = params.lakeFrequency;
        pkt.lakeOctaves = params.lakeOctaves;
        pkt.lakePersistence = params.lakePersistence;
        pkt.lakeLacunarity = params.lakeLacunarity;
        pkt.lakeThreshold = params.lakeThreshold;
        pkt.lakeFeather = params.lakeFeather;
        pkt.lakeDepth = params.lakeDepth;
        pkt.lakeMinContinentalness = params.lakeMinContinentalness;
        pkt.genSize = params.genSize;
        pkt.downsample = params.downsample;
        pkt.continentalnessFrequency = params.continentalnessFrequency;
        pkt.continentalnessOctaves = params.continentalnessOctaves;
        pkt.continentalnessPersistence = params.continentalnessPersistence;
        pkt.continentalnessLacunarity = params.continentalnessLacunarity;
        pkt.continentalnessScalingFactor = params.continentalnessScalingFactor;
        pkt.erosionFrequency = params.erosionFrequency;
        pkt.erosionOctaves = params.erosionOctaves;
        pkt.erosionPersistence = params.erosionPersistence;
        pkt.erosionLacunarity = params.erosionLacunarity;
        pkt.erosionScalingFactor = params.erosionScalingFactor;
        pkt.peakValleyFrequency = params.peakValleyFrequency;
        pkt.peakValleyOctaves = params.peakValleyOctaves;
        pkt.peakValleyPersistence = params.peakValleyPersistence;
        pkt.peakValleyLacunarity = params.peakValleyLacunarity;
        pkt.peakValleyScalingFactor = params.peakValleyScalingFactor;
        pkt.temperatureFrequency = params.temperatureFrequency;
        pkt.temperatureOctaves = params.temperatureOctaves;
        pkt.temperaturePersistence = params.temperaturePersistence;
        pkt.temperatureLacunarity = params.temperatureLacunarity;
        pkt.temperatureScalingFactor = params.temperatureScalingFactor;
        pkt.humidityFrequency = params.humidityFrequency;
        pkt.humidityOctaves = params.humidityOctaves;
        pkt.humidityPersistence = params.humidityPersistence;
        pkt.humidityLacunarity = params.humidityLacunarity;
        pkt.humidityScalingFactor = params.humidityScalingFactor;
        pkt.biomeScaleChunks = params.biomeScaleChunks;
        pkt.snapClimateToCells = params.snapClimateToCells;
        pkt.climateWarpFrequency = params.climateWarpFrequency;
        pkt.climateWarpStrength = params.climateWarpStrength;
        pkt.desertMoistureThreshold = params.desertMoistureThreshold;
        pkt.forestMoistureThreshold = params.forestMoistureThreshold;
        pkt.snowTemperatureThreshold = params.snowTemperatureThreshold;
        pkt.debugOresOnly = params.debugOresOnly;
        
        sendParamsCallback(pkt);
        sendParamsTimer = 0.0f;
    }
    
    ImGui::Separator();
    ImGui::Text("Height Generation (affects preview):");
    
    if (ImGui::SliderInt("Seed", &params.seed, 1, 10000)) {
        dirty = true;
    }
    if (ImGui::SliderInt("Sea Level", &params.seaLevel, 0, 128)) {
        dirty = true;
    }
    
    if (ImGui::TreeNode("Continentalness (Base Height Variation)")) {
        if (ImGui::SliderFloat("Continent Freq", &params.continentalnessFrequency, 0.0001f, 0.01f)) {
            dirty = true;
        }
        if (ImGui::SliderInt("Continent Octaves", &params.continentalnessOctaves, 1, 8)) {
            dirty = true;
        }
        if (ImGui::SliderFloat("Continent Persist", &params.continentalnessPersistence, 0.1f, 1.0f)) {
            dirty = true;
        }
        if (ImGui::SliderFloat("Continent Lacun", &params.continentalnessLacunarity, 1.0f, 4.0f)) {
            dirty = true;
        }
        if (ImGui::SliderFloat("Continent Scale", &params.continentalnessScalingFactor, 0.5f, 10.0f)) {
            dirty = true;
        }
        ImGui::TreePop();
    }
    
    if (ImGui::TreeNode("Erosion (Surface Detail)")) {
        if (ImGui::SliderFloat("Erosion Freq", &params.erosionFrequency, 0.0001f, 0.05f)) {
            dirty = true;
        }
        if (ImGui::SliderInt("Erosion Octaves", &params.erosionOctaves, 1, 8)) {
            dirty = true;
        }
        if (ImGui::SliderFloat("Erosion Persist", &params.erosionPersistence, 0.1f, 1.0f)) {
            dirty = true;
        }
        if (ImGui::SliderFloat("Erosion Lacun", &params.erosionLacunarity, 1.0f, 4.0f)) {
            dirty = true;
        }
        ImGui::TreePop();
    }
    
    ImGui::Separator();
    ImGui::Text("Rivers:");
    bool riverParamChanged = false;
    if (ImGui::SliderFloat("River Freq", &params.riverFrequency, 0.0001f, 0.05f)) {
        dirty = true;
        riverParamChanged = true;
    }
    if (ImGui::SliderInt("River Octaves", &params.riverOctaves, 1, 8)) {
        dirty = true;
        riverParamChanged = true;
    }
    if (ImGui::SliderFloat("River Persist", &params.riverPersistence, 0.1f, 1.0f)) {
        dirty = true;
        riverParamChanged = true;
    }
    if (ImGui::SliderFloat("River Lacun", &params.riverLacunarity, 1.0f, 4.0f)) {
        dirty = true;
        riverParamChanged = true;
    }
    
    if (ImGui::SliderFloat("River Width", &params.riverWidth, 0.001f, 0.1f)) {
        riverParamChanged = true;
    }
    if (ImGui::SliderFloat("River Depth", &params.riverDepth, 1.0f, 64.0f)) {
        riverParamChanged = true;
    }

    if (ImGui::SliderFloat("River Warp Freq", &params.riverWarpFrequency, 0.0001f, 0.05f)) {
        dirty = true;
        riverParamChanged = true;
    }
    if (ImGui::SliderFloat("River Warp Str", &params.riverWarpStrength, 10.0f, 500.0f)) {
        dirty = true;
        riverParamChanged = true;
    }
    if (ImGui::SliderFloat("Min River Cont", &params.riverMinContinentalness, -1.0f, 1.0f)) {
        dirty = true;
        riverParamChanged = true;
    }

    ImGui::Separator();
    ImGui::Text("Lakes:");
    bool lakeParamChanged = false;
    if (ImGui::SliderFloat("Lake Freq", &params.lakeFrequency, 0.0001f, 0.05f)) {
        dirty = true;
        lakeParamChanged = true;
    }
    if (ImGui::SliderInt("Lake Octaves", &params.lakeOctaves, 1, 8)) {
        dirty = true;
        lakeParamChanged = true;
    }
    if (ImGui::SliderFloat("Lake Persist", &params.lakePersistence, 0.1f, 1.0f)) {
        dirty = true;
        lakeParamChanged = true;
    }
    if (ImGui::SliderFloat("Lake Lacun", &params.lakeLacunarity, 1.0f, 4.0f)) {
        dirty = true;
        lakeParamChanged = true;
    }
    if (ImGui::SliderFloat("Lake Thresh", &params.lakeThreshold, 0.0f, 1.0f)) {
        dirty = true;
        lakeParamChanged = true;
    }
    if (ImGui::SliderFloat("Lake Feather", &params.lakeFeather, 0.01f, 0.5f)) {
        dirty = true;
        lakeParamChanged = true;
    }
    if (ImGui::SliderFloat("Lake Depth", &params.lakeDepth, 1.0f, 64.0f)) {
        lakeParamChanged = true;
    }
    if (ImGui::SliderFloat("Min Lake Cont", &params.lakeMinContinentalness, -1.0f, 1.0f)) {
        dirty = true;
        lakeParamChanged = true;
    }
    
    ImGui::End();
}