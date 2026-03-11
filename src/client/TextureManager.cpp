#include "TextureManager.hpp"
#include <filesystem>
#include <iostream>
#include <algorithm>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"


namespace fs = std::filesystem;

TextureManager::~TextureManager() {
    if (textureArray) {
        glDeleteTextures(1, &textureArray);
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

bool TextureManager::loadRessourcePack(const std::string& path, int textureSize) {
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

    // Step2: create GL_TEXTURE_2D_ARRAY
    glGenTextures(1, &textureArray);
    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);

    // allocate storage for all layers
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0 , GL_RGBA8, textureSize, textureSize, layerCount, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);

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

        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0,
                         0, 0, i,                       // x, y, layer
                         textureSize, textureSize, 1,   // width, height, depth
                         GL_RGBA, GL_UNSIGNED_BYTE,
                         pixels.data());

        textureNameToLayer[entries[i].name] = i;
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

int TextureManager::getTextureLayer(const std::string& name) const {
    auto it = textureNameToLayer.find(name);
    if (it != textureNameToLayer.end())
        return it->second;
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
    // Map each blocktype to it's texture names

    auto layer = [this](const std::string& name) -> int{
        return getTextureLayer(name);
    };

    blockTextureMap[BlockType::DIRT]    = BlockTextures::uniform(layer("dirt"));
    blockTextureMap[BlockType::STONE]   = BlockTextures::uniform(layer("stone"));
    blockTextureMap[BlockType::SAND]    = BlockTextures::uniform(layer("sand"));
    blockTextureMap[BlockType::SNOW]    = BlockTextures::uniform(layer("snow"));
    blockTextureMap[BlockType::BEDROCK] = BlockTextures::uniform(layer("bedrock"));
    blockTextureMap[BlockType::LEAVES]  = BlockTextures::uniform(layer("spruce_leaves"));
    blockTextureMap[BlockType::IRON]    = BlockTextures::uniform(layer("iron_ore"));
    blockTextureMap[BlockType::GOLD]    = BlockTextures::uniform(layer("gold_ore"));
    blockTextureMap[BlockType::DIAMOND] = BlockTextures::uniform(layer("diamond_ore"));
    blockTextureMap[BlockType::URANIUM] = BlockTextures::uniform(layer("emerald_ore"));
    blockTextureMap[BlockType::WATER]   = BlockTextures::uniform(layer("water_overlay"));
    
    blockTextureMap[BlockType::GRASS]   = BlockTextures::topBottomSides(
                                            layer("grass_block_top"),
                                            layer("dirt"),
                                            layer("grass_block_side"));
    blockTextureMap[BlockType::LOG]     = BlockTextures::topBottomSides(
                                            layer("spruce_log_top"),
                                            layer("spruce_log_top"),
                                            layer("spruce_log"));
    
}
