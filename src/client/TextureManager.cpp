#include "TextureManager.hpp"
#include "GLFW/glfw3.h"
#include <filesystem>
#include <iostream>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


namespace fs = std::filesystem;

TextureManager::~TextureManager() {
    if (glfwGetCurrentContext()) {
        if (textureArray) {
            glDeleteTextures(1, &textureArray);
        }
    }
}

std::vector<unsigned char> TextureManager::loadImage(const std::string& path, int& width, int& height) {
    int channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4); // Force RGBA
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        return {};
    }
    std::vector<unsigned char> pixels(data, data + width * height * 4);
    stbi_image_free(data);
    return pixels;
}

bool TextureManager::loadResourcePack(const std::string& path, int textureSize) {
    this->textureSize = textureSize;

    std::string textureDir = path + "/textures/block/";
    if (!fs::exists(textureDir)) {
        std::cerr << "Texture directory not found: " << textureDir << std::endl;
        return false;
    }

    // Step 1: collect all .png files
    struct TextureEntry {
        std::string name; // "stone"
        std::string path; // full path to the file
    };
    std::vector<TextureEntry> entries;

    for (const auto& entry : fs::directory_iterator(textureDir)) {
        if (entry.path().extension() == ".png") {
            std::string name = entry.path().stem().string();
            entries.push_back({ name, entry.path().string() });
        }
    }

    if (entries.empty()) {
        std::cerr << "No textures found in: " << textureDir << std::endl;
        return false;
    }

    // sort for deterministic layer ordering
    std::sort(entries.begin(), entries.end(), [](const TextureEntry& a, const TextureEntry& b) {
        return a.name < b.name;
    });

    layerCount = static_cast<int>(entries.size());

    // reserve extra layers for tinted variants
    maxLayers = layerCount + 32;

    // Step2: create GL_TEXTURE_2D_ARRAY
    glGenTextures(1, &textureArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);

    // allocate storage for all layers
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0 , GL_RGBA8, textureSize, textureSize, maxLayers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

    layerPixels.resize(maxLayers); // prepare storage for pixel data of each layer

    // Step 3: load each texture into its layer
    for (int i = 0; i < layerCount; i++) {
        int width, height;
        auto pixels = loadImage(entries[i].path, width, height);
        if (pixels.empty()) continue;

        // Resize if needed (simple nearest-neighbor for pixel art)
        if (width != textureSize || height != textureSize) {
            std::vector<unsigned char> resized(textureSize * textureSize * 4);
            for (int y = 0; y < textureSize; ++y) {
                for (int x = 0; x < textureSize; ++x) {
                    int srcX = x * width / textureSize;
                    int srcY = y * height / textureSize;
                    int srcIdx = (srcY * width + srcX) * 4;
                    int dstIdx = (y * textureSize + x) * 4;
                    resized[dstIdx + 0] = pixels[srcIdx + 0];
                    resized[dstIdx + 1] = pixels[srcIdx + 1];
                    resized[dstIdx + 2] = pixels[srcIdx + 2];
                    resized[dstIdx + 3] = pixels[srcIdx + 3];
                }
            }
            pixels = std::move(resized);
        }

        // Check if texture has any transparency
        bool hasTransparency = false;
        for (size_t t = 3; t < pixels.size(); t += 4) {
            if (pixels[t] < 255) {
                hasTransparency = true;
                break;
            }
        }

        // Only premultiply alpha for textures with transparency to prevent mipmap edge artifacts
        // For opaque textures, leave them unchanged to avoid precision issues
        if (hasTransparency) {
            for (size_t p = 0; p < pixels.size(); p += 4) {
                const float alpha = pixels[p + 3] / 255.0f;
                pixels[p + 0] = static_cast<unsigned char>(pixels[p + 0] * alpha);
                pixels[p + 1] = static_cast<unsigned char>(pixels[p + 1] * alpha);
                pixels[p + 2] = static_cast<unsigned char>(pixels[p + 2] * alpha);
            }
        }

        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                         0, 0, i,                       // x, y, layer
                         textureSize, textureSize, 1,   // width, height, depth
                         GL_RGBA, GL_UNSIGNED_BYTE,
                         pixels.data());

        textureNameToLayer[entries[i].name] = i;
        layerPixels[i] = std::move(pixels);
    }

    // Filtering
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);

    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    // step4: map block types to texture layers
    setupBlockTextureMapping();

    std::cout << "Loaded " << layerCount << " textures into array from: " << textureDir << std::endl;
    return true;
}

int TextureManager::addTintedLayer(const std::string& sourceTexture, unsigned char r, unsigned char g, unsigned char b) {
    // build unique name for tinted variant
    std::string tintedName = sourceTexture + "_tint_"
        + std::to_string(r) + "_"
        + std::to_string(g) + "_"
        + std::to_string(b);

    // if this tinted variant already exists, return its layer
    auto existing = textureNameToLayer.find(tintedName);
    if (existing != textureNameToLayer.end())
        return existing->second;

    // find source layer
    auto srcIt = textureNameToLayer.find(sourceTexture);
    if (srcIt == textureNameToLayer.end()) {
        std::cerr << "Tint error: Source texture not found for tinting: " << sourceTexture << std::endl;
        return 0;
    }

    int srcLayer = srcIt->second;
    if (layerPixels[srcLayer].empty()) {
        std::cerr << "Tint error: No pixel data for source texture layer: " << sourceTexture << std::endl;
        return 0;
    }

    if (layerCount >= maxLayers) {
        std::cerr << "Tint error: Maximum number of texture layers reached, cannot add tinted variant: " << maxLayers << " layers" << std::endl;
        return 0;
    }

    // create tinted pixels
    std::vector<unsigned char> tinted = layerPixels[srcLayer];
    for (size_t i = 0; i < tinted.size(); i += 4) {
        // Note: source already has premultiplied alpha, so we just tint the RGB
        tinted[i + 0] = (tinted[i + 0] * r) / 255;
        tinted[i + 1] = (tinted[i + 1] * g) / 255;
        tinted[i + 2] = (tinted[i + 2] * b) / 255;
        // alpha unchanged
    }

    // upload to next available layer
    int newLayer = layerCount;
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
    glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                     0, 0, newLayer,
                     textureSize, textureSize, 1,
                     GL_RGBA, GL_UNSIGNED_BYTE,
                     tinted.data());
    glGenerateMipmap(GL_TEXTURE_2D_ARRAY);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);

    textureNameToLayer[tintedName] = newLayer;
    layerPixels[newLayer] = std::move(tinted);
    layerCount++;

    return newLayer;
}

int TextureManager::getTextureLayer(const std::string& name) const {
    auto it = textureNameToLayer.find(name);
    if (it != textureNameToLayer.end())
        return it->second;
    // TODO : add magenta checker and return its layer index instead of defaulting to 0
    return 0; // default to layer 0 if not found
}

const BlockTextures& TextureManager::getBlockTextures(BlockType type) const {
    static BlockTextures fallback = BlockTextures::uniform(0); // default to layer 0
    auto it = blockTextureMap.find(type);
    if (it != blockTextureMap.end())
        return it->second;
    return fallback;
}

void TextureManager::bind(GLenum textureUnit) const {
    glActiveTexture(textureUnit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
}

void TextureManager::setupBlockTextureMapping() {
    // Map each blocktype to its texture names

    auto layer = [this](const std::string& name) -> int{
        return getTextureLayer(name);
    };

    // ── Tinted variants ────────────────────────────────────────────
    // Minecraft grass/leaves textures are grayscale — the game multiplies
    // them by a biome color at runtime.  We do it once at load time.

    unsigned char r = 0xff, g = 0x00, b = 0x00; // red tint debug
    // Biome grass tints
    biomeGrassTopLayer[BiomeType::PLAINS] = addTintedLayer("grass_block_top", 0x91, 0xBD, 0x59);
    biomeGrassTopLayer[BiomeType::SAVANNA] = addTintedLayer("grass_block_top", 0xB3, 0xBD, 0x59);
    biomeGrassTopLayer[BiomeType::JUNGLE] = addTintedLayer("grass_block_top", 0x44, 0xB5, 0x33);
    biomeGrassTopLayer[BiomeType::DARK_FOREST] = addTintedLayer("grass_block_top", 0x26, 0x63, 0x1D);
    biomeGrassTopLayer[BiomeType::BIRCH_FOREST] = addTintedLayer("grass_block_top", 0x71, 0xBF,  0x4B);

    // Short grass tints
    biomeShortGrassLayer[BiomeType::PLAINS] = addTintedLayer("grass", 0x91, 0xBD, 0x59);
    biomeShortGrassLayer[BiomeType::SAVANNA] = addTintedLayer("grass", 0xB3, 0xBD, 0x59);
    biomeShortGrassLayer[BiomeType::JUNGLE] = addTintedLayer("grass", 0x44, 0xB5, 0x33);
    biomeShortGrassLayer[BiomeType::DARK_FOREST] = addTintedLayer("grass", 0x26, 0x63, 0x1D);
    biomeShortGrassLayer[BiomeType::BIRCH_FOREST] = addTintedLayer("grass", 0x71, 0xBF,  0x4B);
    
    // Leaves tint
    int oakLeavesTinted = addTintedLayer("oak_leaves", 0x61, 0x99, 0x61);
    int spruceLeavesTinted = addTintedLayer("spruce_leaves", 0x17, 0x2B, 0x17);
    int birchLeavesTinted = addTintedLayer("birch_leaves", 0x44, 0x78, 0x44);
    int jungleLeavesTinted = addTintedLayer("jungle_leaves", 0x48, 0xBE, 0x48);
    int acaciaLeavesTinted = addTintedLayer("acacia_leaves", 0x94, 0xA3, 0x1D);
    int darkOakLeavesTinted = addTintedLayer("dark_oak_leaves", 0x4E, 0x96, 0x4E);
    // other Tints
    int waterTinted = addTintedLayer("water_overlay", 0x64, 0x64, 0xFF);

    struct UniformEntry {
        BlockType type;
        int layer;
    };

    UniformEntry uniformBlocks[] = {
        { BlockType::DIRT,                  layer("dirt") },
        { BlockType::STONE,                 layer("stone") },
        { BlockType::SAND,                  layer("sand") },
        { BlockType::SNOW,                  layer("snow") },
        { BlockType::BEDROCK,               layer("bedrock") },
        { BlockType::OAK_LEAVES,            oakLeavesTinted },
        { BlockType::SPRUCE_LEAVES,         spruceLeavesTinted },
        { BlockType::BIRCH_LEAVES,          birchLeavesTinted },
        { BlockType::JUNGLE_LEAVES,         jungleLeavesTinted },
        { BlockType::ACACIA_LEAVES,         acaciaLeavesTinted },
        { BlockType::DARK_OAK_LEAVES,       darkOakLeavesTinted },
        { BlockType::IRON,                  layer("iron_ore") },
        { BlockType::GOLD,                  layer("gold_ore") },
        { BlockType::DIAMOND,               layer("diamond_ore") },
        { BlockType::URANIUM,               layer("emerald_ore") },
        { BlockType::WATER,                 waterTinted },
        { BlockType::SHORT_GRASS,           biomeShortGrassLayer[BiomeType::PLAINS] }, // will be overridden by biome-specific tints in shader
        { BlockType::CORNFLOWER,            layer("cornflower") },
        { BlockType::POPPY,                 layer("poppy") },
        { BlockType::PINK_TULIP,            layer("pink_tulip") },
        { BlockType::WHITE_TULIP,           layer("white_tulip") },
        { BlockType::ORANGE_TULIP,          layer("orange_tulip") },
        { BlockType::RED_TULIP,             layer("red_tulip") },
        { BlockType::ALLIUM,                layer("allium") },
        { BlockType::AZURE_BLUET,           layer("azure_bluet") },
        { BlockType::BLUE_ORCHID,           layer("blue_orchid") },
        { BlockType::DANDELION,             layer("dandelion") },
        { BlockType::LILY_OF_THE_VALLEY,    layer("lily_of_the_valley") },
        { BlockType::OXEYE_DAISY,           layer("oxeye_daisy") },
        { BlockType::RED_MUSHROOM,          layer("red_mushroom") },
        { BlockType::BROWN_MUSHROOM,        layer("brown_mushroom") },
        { BlockType::WITHER_ROSE,           layer("wither_rose") },
        { BlockType::SEAGRASS,              layer("seagrass") },
        { BlockType::TALL_SEAGRASS_BOTTOM,  layer("tall_seagrass_bottom") },
        { BlockType::TALL_SEAGRASS_TOP,     layer("tall_seagrass_top") },
        { BlockType::KELP,                  layer("kelp") },
        { BlockType::KELP_PLANT,            layer("kelp_plant") },
        { BlockType::BRAIN_CORAL,           layer("brain_coral") },
        { BlockType::BRAIN_CORAL_FAN,       layer("brain_coral_fan") },
        { BlockType::BUBBLE_CORAL,          layer("bubble_coral") },
        { BlockType::BUBBLE_CORAL_FAN,      layer("bubble_coral_fan") },
        { BlockType::FIRE_CORAL,            layer("fire_coral") },
        { BlockType::FIRE_CORAL_FAN,        layer("fire_coral_fan") },
        { BlockType::HORN_CORAL,            layer("horn_coral") },
        { BlockType::HORN_CORAL_FAN,        layer("horn_coral_fan") },
        { BlockType::TUBE_CORAL,            layer("tube_coral") },
        { BlockType::TUBE_CORAL_FAN,        layer("tube_coral_fan") },
        { BlockType::DEAD_BUSH,             layer("dead_bush") },
        { BlockType::TERRACOTTA,            layer("terracotta") },
        { BlockType::RED_TERRACOTTA,        layer("red_terracotta") },
        { BlockType::GRAY_TERRACOTTA,       layer("gray_terracotta") },
        { BlockType::PINK_TERRACOTTA,       layer("pink_terracotta") },
        { BlockType::BLACK_TERRACOTTA,      layer("black_terracotta") },
        { BlockType::BROWN_TERRACOTTA,      layer("brown_terracotta") },
        { BlockType::WHITE_TERRACOTTA,      layer("white_terracotta") },
        { BlockType::ORANGE_TERRACOTTA,     layer("orange_terracotta") },
        { BlockType::YELLOW_TERRACOTTA,     layer("yellow_terracotta") },
        { BlockType::LIGHT_GRAY_TERRACOTTA, layer("light_gray_terracotta") },
    };
    for (const auto& [type, l] : uniformBlocks) {
        blockTextureMap[type] = BlockTextures::uniform(l);
    }
    
    blockTextureMap[BlockType::GRASS]   = BlockTextures::topBottomSides(
                                            biomeGrassTopLayer[BiomeType::PLAINS],
                                            layer("dirt"),
                                            layer("grass_block_side"));

    blockTextureMap[BlockType::OAK_LOG]     = BlockTextures::topBottomSides(
                                            layer("oak_log_top"),
                                            layer("oak_log_top"),
                                            layer("oak_log"));

    blockTextureMap[BlockType::BIRCH_LOG]   = BlockTextures::topBottomSides(
                                            layer("birch_log_top"),
                                            layer("birch_log_top"),
                                            layer("birch_log"));

    blockTextureMap[BlockType::ACACIA_LOG] = BlockTextures::topBottomSides(
                                            layer("acacia_log_top"),
                                            layer("acacia_log_top"),
                                            layer("acacia_log"));

    blockTextureMap[BlockType::JUNGLE_LOG] = BlockTextures::topBottomSides(
                                            layer("jungle_log_top"),
                                            layer("jungle_log_top"),
                                            layer("jungle_log"));

    blockTextureMap[BlockType::SPRUCE_LOG] = BlockTextures::topBottomSides(
                                            layer("spruce_log_top"),
                                            layer("spruce_log_top"),
                                            layer("spruce_log"));

    blockTextureMap[BlockType::DARK_OAK_LOG] = BlockTextures::topBottomSides(
                                            layer("dark_oak_log_top"),
                                            layer("dark_oak_log_top"),
                                            layer("dark_oak_log"));

    blockTextureMap[BlockType::CACTUS]     = BlockTextures::topBottomSides(
                                            layer("cactus_top"),
                                            layer("cactus_bottom"),
                                            layer("cactus_side"));
}

int TextureManager::getGrassTintLayer(BiomeType biome) const {
    auto it = biomeGrassTopLayer.find(biome);
    return it != biomeGrassTopLayer.end() ? it->second : biomeGrassTopLayer.at(BiomeType::PLAINS); // default to plains tint if biome not found
}

int TextureManager::getShortGrassTintLayer(BiomeType biome) const {
    auto it = biomeShortGrassLayer.find(biome);
    return it != biomeShortGrassLayer.end() ? it->second : biomeShortGrassLayer.at(BiomeType::PLAINS); // default to plains tint if biome not found
}