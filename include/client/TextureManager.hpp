#ifndef TEXTURE_MANAGER_HPP
#define TEXTURE_MANAGER_HPP

#include <glad/glad.h>
#include <string>
#include <unordered_map>
#include "Item.hpp"

enum class BlockFace {
    FRONT,  // +Z
    BACK,   // -Z
    TOP,    // +Y
    BOTTOM, // -Y
    RIGHT,  // +X
    LEFT    // -X
};

struct BlockTextures {
    int top;
    int bottom;
    int north;  // +Z
    int south;  // -Z
    int east;   // +X
    int west;   // -X

    // All faces the same
    static BlockTextures uniform(int layer) {
        return { layer, layer, layer, layer, layer, layer };
    }

    // Top and bottom faces different from sides
    static BlockTextures topBottomSides(int top, int bottom, int side) {
        return { top, bottom, side, side, side, side };
    }

    //TODO: add constructor for furnace for example (different textures for front/back and top/bottom)

    // Get layer for a face index (0=front/+Z, 1=back/-Z, 2=top, 3=bottom, 4=right/+X, 5=left/-X)
    int getLayerForFace(int face) const {
        switch (face) {
            case 0: return north;   // FRONT (+Z)
            case 1: return south;   // BACK  (-Z)
            case 2: return top;     // TOP   (+Y)
            case 3: return bottom;  // BOTTOM(-Y)
            case 4: return east;    // RIGHT (+X)
            case 5: return west;    // LEFT  (-X)
            default: return top;
        }
    }
};

class TextureManager {
    public:
        TextureManager() = default;
        ~TextureManager();

        // TODO: check texturesize from texture metadata (width / height)
        bool loadResourcePack(const std::string& path, int textureSize = 16);

        // Get the GL texture array handle
        GLuint getTextureArray() const { return textureArray; }

        // Get the layer index for a named texture (e.g., "grass_top", "dirt_side", etc.)
        int getTextureLayer(const std::string& name) const;

        // Get the textures for a block type
        const BlockTextures& getBlockTextures(BlockType type) const;

        // Get the atlas layer used to draw an item as a flat sprite (drop entity
        // or inventory icon). For vegetation blocks this is the block's texture
        // layer; for weapons/misc this comes from their texturePath in the
        // ItemRegistry. Returns 0 if the texture wasn't found.
        int getItemSpriteLayer(const ItemType& type) const;

        void bind(GLenum textureUnit = GL_TEXTURE0) const;

        int getGrassTintLayer(BiomeType biome) const;
        int getShortGrassTintLayer(BiomeType biome) const;

    private:
        GLuint textureArray { 0 };
        int textureSize { 16 };
        int layerCount { 0 };
        int maxLayers { 0 }; // total layers including tinted variants

        std::unordered_map<std::string, int> textureNameToLayer; // Map texture name to layer index

        std::unordered_map<BlockType, BlockTextures> blockTextureMap; // Map block type

        // Item (non-block) sprite layer, keyed by ItemID (works for both
        // WeaponType and MiscType because their numeric ranges are disjoint).
        std::unordered_map<ItemID, int> itemSpriteLayerMap;

        // load a single image file and return raw RGBA pixels
        std::vector<unsigned char> loadImage(const std::string& path, int& width, int& height);

        // Define which textures each block type uses
        void setupBlockTextureMapping();
        // Define which texture each non-block item uses (weapons, misc).
        void setupItemTextureMapping();

        // tint a texture and upload it as a new layer return the new layer index
        int addTintedLayer(const std::string& sourceTexture, unsigned char r, unsigned char g, unsigned char b);

        // store raw pixels per layer so we can tint them later
        std::vector<std::vector<unsigned char>> layerPixels;

        std::unordered_map<BiomeType, int> biomeGrassTopLayer;
        std::unordered_map<BiomeType, int> biomeShortGrassLayer;
};

#endif // TEXTURE_MANAGER_HPP
