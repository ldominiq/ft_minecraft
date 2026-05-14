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
        // Returns the dedicated "missing texture" layer (a magenta/black checker)
        // when the name isn't registered, so missing assets are visually obvious.
        int getTextureLayer(const std::string& name) const;

        // Get the textures for a block type. Falls back to the magenta/black
        // "missing texture" checker on all faces if the type isn't mapped.
        const BlockTextures& getBlockTextures(BlockType type) const;

        // Get the atlas layer used to draw an item as a flat sprite (drop entity
        // or inventory icon). For vegetation blocks this is the block's texture
        // layer; for weapons/misc this comes from their texturePath in the
        // ItemRegistry. Returns the "missing texture" layer if not found, so
        // unassigned items show up as a magenta/black checker rather than a
        // random sibling texture.
        int getItemSpriteLayer(const ItemType& type) const;

        // Layer index of the magenta/black checker reserved for missing
        // assets. Useful for call sites that want to detect/skip rendering.
        int getMissingTextureLayer() const { return missingTextureLayer; }

        // Raw pre-multiplied RGBA bytes (textureSize × textureSize × 4) for a
        // given atlas layer, or an empty vector if the layer is out of range.
        // Used by HeldItemRenderer to extrude weapon sprites into per-pixel
        // voxel meshes so swords look like real 3D objects in 1P
        const std::vector<unsigned char>& getLayerPixels(int layer) const;
        int getTextureSize() const { return textureSize; }

        void bind(GLenum textureUnit = GL_TEXTURE0) const;

        int getGrassTintLayer(BiomeType biome) const;
        int getShortGrassTintLayer(BiomeType biome) const;

    private:
        GLuint textureArray { 0 };
        int textureSize { 16 };
        int layerCount { 0 };
        int maxLayers { 0 }; // total layers including tinted variants

        // Layer reserved for the "missing texture" magenta/black checker.
        // Filled in by loadResourcePack(); used as the fallback for every
        // lookup that fails so missing assets render obviously broken.
        int missingTextureLayer { 0 };
        // Pre-built BlockTextures pointing every face at missingTextureLayer,
        // returned by reference from getBlockTextures() on a miss.
        BlockTextures fallbackBlockTextures = BlockTextures::uniform(0);

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
