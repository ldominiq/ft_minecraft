#include "ui/TerrainDebugWindow.hpp"
#include "TerrainParams.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <limits>

// ─────────────────────────────────────────────────────────────────────────────
// Constructor / Destructor
// ─────────────────────────────────────────────────────────────────────────────

TerrainDebugWindow::TerrainDebugWindow()
    : texSize(256), dirty(true), regenerateCallback(nullptr), regenerateUserData(nullptr),
    noiseBase(1337),
    noiseErosion(1337 + 237),
    noisePV(1337 + 98789),
    noiseRiver(1337 + 7717),
    noiseRiverWarpX(1337 + 7718),
    noiseRiverWarpZ(1337 + 7719),
    noiseLake(1337 + 9901)
{
    glGenTextures(1, &previewTextureID);
    glBindTexture(GL_TEXTURE_2D, previewTextureID);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
}

void TerrainDebugWindow::resizeTexture(int newSize) {
    texSize = newSize;
    glBindTexture(GL_TEXTURE_2D, previewTextureID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texSize, texSize, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    dirty = true;
}

TerrainDebugWindow::~TerrainDebugWindow() {
    glDeleteTextures(1, &previewTextureID);
}

void TerrainDebugWindow::setRegenerateCallback(void(*callback)(void*), void* userData) {
    regenerateCallback = callback;
    this->regenerateUserData = userData;
}

// ─────────────────────────────────────────────────────────────────────────────
// Noise helpers — mirror ChunkGeneration private methods exactly
// ─────────────────────────────────────────────────────────────────────────────

void TerrainDebugWindow::reseedNoise(int32_t seed) {
    noiseBase.setSeed(seed);
    noiseErosion.setSeed(seed + 237);
    noisePV.setSeed(seed + 98789);
    noiseRiver.setSeed(seed + 7717);
    noiseRiverWarpX.setSeed(seed + 7718);
    noiseRiverWarpZ.setSeed(seed + 7719);
    noiseLake.setSeed(seed + 9901);
    cachedSeed = seed;
}

float TerrainDebugWindow::interpolateSpline(float noise, const std::vector<std::pair<float,float>>& spline) {
    if (noise <= spline.front().first) return spline.front().second;
    if (noise >= spline.back().first)  return spline.back().second;
    for (size_t i = 1; i < spline.size(); ++i) {
        if (noise < spline[i].first) {
            float t = (noise - spline[i-1].first) / (spline[i].first - spline[i-1].first);
            return spline[i-1].second + t * (spline[i].second - spline[i-1].second);
        }
    }
    return spline.back().second;
}

float TerrainDebugWindow::sampleContinentalness(const TerrainGenerationParams& p, float wx, float wz) {
    float fbm = noiseBase.fractalBrownianMotion2D(
        wx * p.continentalnessFrequency,
        wz * p.continentalnessFrequency,
        p.continentalnessOctaves,
        p.continentalnessLacunarity,
        p.continentalnessPersistence
    );
    return glm::clamp(fbm * p.continentalnessScalingFactor, -3.8f, 3.8f);
}

float TerrainDebugWindow::sampleErosion(const TerrainGenerationParams& p, float wx, float wz) {
    float erosion = noiseErosion.fractalBrownianMotion2D(
        wx * p.erosionFrequency,
        wz * p.erosionFrequency,
        p.erosionOctaves,
        p.erosionLacunarity,
        p.erosionPersistence
    );
    return glm::clamp(erosion * p.erosionScalingFactor, -1.0f, 1.0f);
}

float TerrainDebugWindow::samplePV(const TerrainGenerationParams& p, float wx, float wz) {
    float pv = noisePV.fractalBrownianMotion2D(
        wx * p.peakValleyFrequency,
        wz * p.peakValleyFrequency,
        p.peakValleyOctaves,
        p.peakValleyLacunarity,
        p.peakValleyPersistence
    );
    return glm::clamp(pv * p.peakValleyScalingFactor, -1.0f, 1.0f);
}

// Returns final surface height (float) and writes out masks for coloring.
float TerrainDebugWindow::computeHeight(const TerrainGenerationParams& p, float wx, float wz,
                                        float& outCont, float& outRiver, float& outLake) {
    // --- Noise fields ---
    const float continentalness = sampleContinentalness(p, wx, wz);
    const float erosion         = sampleErosion(p, wx, wz);
    const float pv              = samplePV(p, wx, wz);

    outCont = continentalness;

    // --- Spline lookup ---
    const float baseHeight       = interpolateSpline(continentalness, continentalnessSpline);
    const float erosionSplineVal = interpolateSpline(erosion, erosionSpline);
    const float pvSplineVal      = interpolateSpline(pv, peakValleySpline);

    // --- Erosion delta (same logic as ChunkGeneration::computeTerrainHeight) ---
    float eroMin =  std::numeric_limits<float>::infinity();
    float eroMax = -std::numeric_limits<float>::infinity();
    for (const auto& pt : erosionSpline) {
        eroMin = std::min(eroMin, pt.second);
        eroMax = std::max(eroMax, pt.second);
    }

    float erosionNorm = 0.0f;
    if (eroMax > eroMin)
        erosionNorm = glm::clamp((erosionSplineVal - eroMin) / (eroMax - eroMin), 0.0f, 1.0f);
    erosionNorm = 1.0f - erosionNorm;

    const float inlandMask      = glm::smoothstep(-0.19f, 3.8f, continentalness);
    constexpr float minErosionStrength = 2.0f;
    constexpr float maxErosionStrength = 140.0f;
    const float erosionStrength = glm::mix(minErosionStrength, maxErosionStrength, inlandMask);
    const float erosionDelta    = erosionNorm * erosionStrength;

    const float pvFactor = pvSplineVal * (1.0f - erosionNorm);

    float finalHeight = baseHeight - erosionDelta + pvFactor;

    // --- River mask ---
    const float warpX = noiseRiverWarpX.fractalBrownianMotion2D(
        wx * p.riverWarpFrequency, wz * p.riverWarpFrequency, 3, 2.0f, 0.5f
    ) * p.riverWarpStrength;
    const float warpZ = noiseRiverWarpZ.fractalBrownianMotion2D(
        wx * p.riverWarpFrequency, wz * p.riverWarpFrequency, 3, 2.0f, 0.5f
    ) * p.riverWarpStrength;
    const float river = noiseRiver.fractalBrownianMotion2D(
        (wx + warpX) * p.riverFrequency, (wz + warpZ) * p.riverFrequency,
        p.riverOctaves, p.riverLacunarity, p.riverPersistence
    );

    float riverCenter = 1.0f - glm::smoothstep(p.riverWidth, p.riverWidth + p.riverBankFeather, std::abs(river));
    riverCenter = std::pow(glm::clamp(riverCenter, 0.0f, 1.0f), 1.5f);

    const float riverInland      = glm::smoothstep(p.riverMinContinentalness, p.riverMinContinentalness + 0.24f, continentalness);
    const float riverMtnContBlk  = 1.0f - glm::smoothstep(p.riverMaxContinentalness - 0.2f, p.riverMaxContinentalness, continentalness);
    const float riverLowland     = glm::smoothstep((float)p.seaLevel + 2.0f, (float)p.seaLevel + 30.0f, finalHeight);
    const float riverMtnBlk      = 1.0f - glm::smoothstep((float)p.seaLevel + 45.0f, (float)p.seaLevel + 95.0f, finalHeight);
    const float riverSteep       = 1.0f - glm::smoothstep(0.35f, 0.85f, std::abs(pv));

    outRiver = riverCenter * riverInland * riverMtnContBlk * riverLowland * riverMtnBlk * riverSteep;

    if (outRiver > 0.0f) {
        const float bankMask = std::pow(outRiver, 0.45f);
        finalHeight -= p.riverDepth * 0.85f * outRiver;
        finalHeight -= p.riverDepth * 0.60f * bankMask;
        if (outRiver > 0.72f) {
            const float t = glm::clamp((outRiver - 0.72f) / 0.28f, 0.0f, 1.0f);
            finalHeight = glm::mix(finalHeight, (float)p.seaLevel - 1.5f, t);
        }
    }

    // --- Lake mask ---
    const float lakeRaw = noiseLake.fractalBrownianMotion2D(
        wx * p.lakeFrequency, wz * p.lakeFrequency,
        p.lakeOctaves, p.lakeLacunarity, p.lakePersistence
    );
    const float lake01 = (lakeRaw + 1.0f) * 0.5f;

    float lakeCore          = glm::smoothstep(p.lakeThreshold, p.lakeThreshold + p.lakeFeather, lake01);
    const float lakeInland      = glm::smoothstep(p.lakeMinContinentalness, p.lakeMinContinentalness + 0.22f, continentalness);
    const float lakeMtnContBlk  = 1.0f - glm::smoothstep(p.lakeMaxContinentalness - 0.15f, p.lakeMaxContinentalness, continentalness);
    const float lakeFlat        = 1.0f - glm::smoothstep(0.28f, 0.90f, std::abs(pv));
    const float lakeAlt         = glm::smoothstep((float)p.seaLevel + 2.0f, (float)p.seaLevel + 52.0f, finalHeight);

    outLake = lakeCore * lakeInland * lakeMtnContBlk * lakeFlat * lakeAlt;

    if (outLake > 0.0f) {
        const float basinMask = std::pow(outLake, 0.65f);
        finalHeight -= p.lakeDepth * 0.90f * outLake;
        finalHeight -= p.lakeDepth * 0.55f * basinMask;
        if (outLake > 0.70f) {
            const float t = glm::clamp((outLake - 0.70f) / 0.30f, 0.0f, 1.0f);
            finalHeight = glm::mix(finalHeight, (float)p.seaLevel - 1.5f, t);
        }
    }

    return finalHeight;
}

// ─────────────────────────────────────────────────────────────────────────────
// Texture generation
// ─────────────────────────────────────────────────────────────────────────────

void TerrainDebugWindow::updateTexture(const TerrainGenerationParams& params) {
    // Reseed if the seed slider moved
    if (params.seed != cachedSeed)
        reseedNoise(params.seed);

    std::vector<uint32_t> pixels(texSize * texSize);

    const float blocksPerPixel = (viewRangeChunks * 16.0f) / (float)texSize;
    const float sl = (float)params.seaLevel;

    for (int py = 0; py < texSize; ++py) {
        for (int px = 0; px < texSize; ++px) {
            const float wx = (float)camX + ((float)px - texSize * 0.5f) * blocksPerPixel;
            const float wz = (float)camZ + ((float)py - texSize * 0.5f) * blocksPerPixel;

            float cont, riverMask, lakeMask;
            const float h = computeHeight(params, wx, wz, cont, riverMask, lakeMask);

            uint8_t r, g, b;

            if (h < sl) {
                // Below sea level — actual water. Distinguish river/lake from plain ocean.
                if (riverMask > 0.05f) {
                    // River water — vivid cyan-blue
                    float t = glm::clamp(riverMask, 0.0f, 1.0f);
                    r = (uint8_t)(20  + (1.0f - t) * 30);
                    g = (uint8_t)(120 + t * 60);
                    b = (uint8_t)(230 - t * 30);
                } else if (lakeMask > 0.05f) {
                    // Lake water — teal
                    float t = glm::clamp(lakeMask, 0.0f, 1.0f);
                    r = (uint8_t)(20  + (1.0f - t) * 20);
                    g = (uint8_t)(170 - t * 50);
                    b = (uint8_t)(200 - t * 20);
                } else if (h < sl - 20.0f) {
                    // Deep ocean
                    float t = glm::clamp((h - 40.0f) / std::max(1.0f, sl - 20.0f - 40.0f), 0.0f, 1.0f);
                    r = (uint8_t)(10 + t * 20);
                    g = (uint8_t)(30 + t * 50);
                    b = (uint8_t)(80 + t * 80);
                } else {
                    // Shallow ocean
                    float t = glm::clamp((h - (sl - 20.0f)) / 20.0f, 0.0f, 1.0f);
                    r = (uint8_t)(30  + t * 40);
                    g = (uint8_t)(80  + t * 70);
                    b = (uint8_t)(160 + t * 60);
                }
            } else if (h < sl + 3.0f) {
                // Beach / sand
                r = 210; g = 190; b = 130;
            } else if (h < sl + 30.0f) {
                // Lowland / plains
                float t = glm::clamp((h - (sl + 3.0f)) / 27.0f, 0.0f, 1.0f);
                r = (uint8_t)(80  - t * 30);
                g = (uint8_t)(160 + t * 20);
                b = (uint8_t)(60  - t * 20);
            } else if (h < sl + 80.0f) {
                // Forest / hills
                float t = glm::clamp((h - (sl + 30.0f)) / 50.0f, 0.0f, 1.0f);
                r = (uint8_t)(50  + t * 30);
                g = (uint8_t)(130 - t * 50);
                b = (uint8_t)(40  - t * 10);
            } else if (h < sl + 140.0f) {
                // Mountain rock
                float t = glm::clamp((h - (sl + 80.0f)) / 60.0f, 0.0f, 1.0f);
                r = (uint8_t)(100 + t * 100);
                g = (uint8_t)(90  + t * 90);
                b = (uint8_t)(80  + t * 80);
            } else {
                // Snow peaks
                float t = glm::clamp((h - (sl + 140.0f)) / 80.0f, 0.0f, 1.0f);
                uint8_t v = (uint8_t)(200 + t * 55);
                r = g = b = v;
            }

            pixels[py * texSize + px] = (uint32_t)r | ((uint32_t)g << 8) | ((uint32_t)b << 16) | (0xFFu << 24);
        }
    }

    glBindTexture(GL_TEXTURE_2D, previewTextureID);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, texSize, texSize, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    dirty = false;
}

// ─────────────────────────────────────────────────────────────────────────────
// ImGui render
// ─────────────────────────────────────────────────────────────────────────────

void TerrainDebugWindow::render(TerrainGenerationParams& params) {
    if (dirty) updateTexture(params);

    sendParamsTimer += ImGui::GetIO().DeltaTime * 1000.0f;

    ImGui::Begin("Terrain Debugger");

    // ── Navigation ──────────────────────────────────────────────────────────
    ImGui::Text("View Navigation");

    int camChunkX = camX / 16;
    int camChunkZ = camZ / 16;
    bool navChanged = false;

    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::DragInt("Center Chunk X", &camChunkX, 1.0f)) { camX = camChunkX * 16; navChanged = true; }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::DragInt("Center Chunk Z", &camChunkZ, 1.0f)) { camZ = camChunkZ * 16; navChanged = true; }

    ImGui::SetNextItemWidth(200.0f);
    if (ImGui::SliderInt("View range (chunks)", &viewRangeChunks, 10, 2000)) navChanged = true;

    // Step buttons — jump by a quarter of the view range
    const int step = std::max(1, viewRangeChunks / 4);
    if (ImGui::Button("W<<"))  { camX -= step * 4 * 16; navChanged = true; } ImGui::SameLine();
    if (ImGui::Button("W<"))   { camX -= step * 16;     navChanged = true; } ImGui::SameLine();
    if (ImGui::Button("N^"))   { camZ -= step * 16;     navChanged = true; } ImGui::SameLine();
    if (ImGui::Button("Sv"))   { camZ += step * 16;     navChanged = true; } ImGui::SameLine();
    if (ImGui::Button("E>"))   { camX += step * 16;     navChanged = true; } ImGui::SameLine();
    if (ImGui::Button("E>>"))  { camX += step * 4 * 16; navChanged = true; }
    if (ImGui::Button("Reset view")) { camX = 0; camZ = 0; viewRangeChunks = 200; navChanged = true; }

    // High-res toggle — reallocates texture if changed
    bool newHighRes = highRes;
    if (ImGui::Checkbox("High Res (512x512, slower)", &newHighRes) && newHighRes != highRes) {
        highRes = newHighRes;
        resizeTexture(highRes ? 512 : 256);
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(120.0f);
    if (ImGui::SliderInt("Display px", &displaySize, 256, 1024)) { /* no texture regen needed */ }

    if (navChanged) dirty = true;

    // ── Preview image ────────────────────────────────────────────────────────
    const float halfRange = viewRangeChunks / 2.0f;
    ImGui::Text("Preview %dx%d  |  chunks [%d..%d] x [%d..%d]",
        texSize, texSize,
        camChunkX - (int)halfRange, camChunkX + (int)halfRange,
        camChunkZ - (int)halfRange, camChunkZ + (int)halfRange);
    ImGui::TextDisabled("CyanBlue=River  Teal=Lake  DkBlue=Ocean  Tan=Beach  Green=Lowland  DkGreen=Hills  Gray=Mtn  White=Peak");
    ImGui::TextDisabled("River/Lake colors only appear where finalHeight < seaLevel (actual water). Zoom <30 chunks for river detail.");

    ImGui::Image((void*)(intptr_t)previewTextureID, ImVec2((float)displaySize, (float)displaySize));

    ImGui::Separator();

    // ── Action buttons ───────────────────────────────────────────────────────
    ImGui::TextDisabled("Regenerate World reloads all chunks. Sync to Server first.");
    if (ImGui::Button("Regenerate World") && regenerateCallback)
        regenerateCallback(regenerateUserData);
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
        pkt.riverMaxContinentalness = params.riverMaxContinentalness;
        pkt.lakeFrequency = params.lakeFrequency;
        pkt.lakeOctaves = params.lakeOctaves;
        pkt.lakePersistence = params.lakePersistence;
        pkt.lakeLacunarity = params.lakeLacunarity;
        pkt.lakeThreshold = params.lakeThreshold;
        pkt.lakeFeather = params.lakeFeather;
        pkt.lakeDepth = params.lakeDepth;
        pkt.lakeMinContinentalness = params.lakeMinContinentalness;
        pkt.lakeMaxContinentalness = params.lakeMaxContinentalness;
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

    // ── Terrain parameters ───────────────────────────────────────────────────
    ImGui::Text("Height Generation (affects preview):");

    if (ImGui::SliderInt("Seed", &params.seed, 1, 10000)) dirty = true;
    if (ImGui::SliderInt("Sea Level", &params.seaLevel, 0, 128)) dirty = true;

    if (ImGui::TreeNode("Continentalness")) {
        ImGui::TextDisabled("Scale of ocean/land masses. Lower freq = bigger continents.");
        if (ImGui::SliderFloat("Continent Freq", &params.continentalnessFrequency, 0.0001f, 0.01f)) dirty = true;
        if (ImGui::SliderInt("Continent Octaves", &params.continentalnessOctaves, 1, 8)) dirty = true;
        if (ImGui::SliderFloat("Continent Persist", &params.continentalnessPersistence, 0.1f, 1.0f)) dirty = true;
        if (ImGui::SliderFloat("Continent Lacun", &params.continentalnessLacunarity, 1.0f, 4.0f)) dirty = true;
        if (ImGui::SliderFloat("Continent Scale", &params.continentalnessScalingFactor, 0.5f, 10.0f)) dirty = true;
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Erosion")) {
        ImGui::TextDisabled("Inland/mountain erosion detail. Higher freq = more choppy terrain.");
        if (ImGui::SliderFloat("Erosion Freq", &params.erosionFrequency, 0.0001f, 0.05f)) dirty = true;
        if (ImGui::SliderInt("Erosion Octaves", &params.erosionOctaves, 1, 8)) dirty = true;
        if (ImGui::SliderFloat("Erosion Persist", &params.erosionPersistence, 0.1f, 1.0f)) dirty = true;
        if (ImGui::SliderFloat("Erosion Lacun", &params.erosionLacunarity, 1.0f, 4.0f)) dirty = true;
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Peak/Valley")) {
        ImGui::TextDisabled("Mountains vs valleys on inland terrain.");
        if (ImGui::SliderFloat("PV Freq", &params.peakValleyFrequency, 0.0001f, 0.01f)) dirty = true;
        if (ImGui::SliderInt("PV Octaves", &params.peakValleyOctaves, 1, 8)) dirty = true;
        if (ImGui::SliderFloat("PV Persist", &params.peakValleyPersistence, 0.1f, 1.0f)) dirty = true;
        if (ImGui::SliderFloat("PV Lacun", &params.peakValleyLacunarity, 1.0f, 4.0f)) dirty = true;
        ImGui::TreePop();
    }

    ImGui::Separator();

    if (ImGui::TreeNode("Rivers")) {
        ImGui::TextDisabled("Freq: density of river network. Width: channel width (noise space).");
        ImGui::TextDisabled("WarpStrength: how much rivers meander. MinCont: how far inland rivers start.");
        ImGui::TextDisabled("Zoom view to <30 chunks to see individual river channels.");
        if (ImGui::SliderFloat("River Freq", &params.riverFrequency, 0.0001f, 0.05f)) dirty = true;
        if (ImGui::SliderInt("River Octaves", &params.riverOctaves, 1, 8)) dirty = true;
        if (ImGui::SliderFloat("River Persist", &params.riverPersistence, 0.1f, 1.0f)) dirty = true;
        if (ImGui::SliderFloat("River Lacun", &params.riverLacunarity, 1.0f, 4.0f)) dirty = true;
        if (ImGui::SliderFloat("River Width", &params.riverWidth, 0.001f, 0.2f)) dirty = true;
        if (ImGui::SliderFloat("River Bank Feather", &params.riverBankFeather, 0.001f, 0.2f)) dirty = true;
        if (ImGui::SliderFloat("River Depth", &params.riverDepth, 1.0f, 64.0f)) dirty = true;
        if (ImGui::SliderFloat("River Warp Freq", &params.riverWarpFrequency, 0.0001f, 0.05f)) dirty = true;
        if (ImGui::SliderFloat("River Warp Str", &params.riverWarpStrength, 10.0f, 500.0f)) dirty = true;
        if (ImGui::SliderFloat("Min River Cont", &params.riverMinContinentalness, -1.0f, 1.0f)) dirty = true;
        if (ImGui::SliderFloat("Max River Cont", &params.riverMaxContinentalness, -1.0f, 3.8f)) dirty = true;
        ImGui::TreePop();
    }

    if (ImGui::TreeNode("Lakes")) {
        ImGui::TextDisabled("Threshold: higher = rarer lakes. Lakes only appear on flat inland terrain.");
        if (ImGui::SliderFloat("Lake Freq", &params.lakeFrequency, 0.0001f, 0.05f)) dirty = true;
        if (ImGui::SliderInt("Lake Octaves", &params.lakeOctaves, 1, 8)) dirty = true;
        if (ImGui::SliderFloat("Lake Persist", &params.lakePersistence, 0.1f, 1.0f)) dirty = true;
        if (ImGui::SliderFloat("Lake Lacun", &params.lakeLacunarity, 1.0f, 4.0f)) dirty = true;
        if (ImGui::SliderFloat("Lake Thresh", &params.lakeThreshold, 0.0f, 1.0f)) dirty = true;
        if (ImGui::SliderFloat("Lake Feather", &params.lakeFeather, 0.01f, 0.5f)) dirty = true;
        if (ImGui::SliderFloat("Lake Depth", &params.lakeDepth, 1.0f, 64.0f)) dirty = true;
        if (ImGui::SliderFloat("Min Lake Cont", &params.lakeMinContinentalness, -1.0f, 1.0f)) dirty = true;
        if (ImGui::SliderFloat("Max Lake Cont", &params.lakeMaxContinentalness, -1.0f, 3.8f)) dirty = true;
        ImGui::TreePop();
    }

    ImGui::End();
}
