#pragma once

// Named texture unit indices used across the renderer.
// Usage:
//   glActiveTexture(GL_TEXTURE0 + TextureUnits::SKY_LUT);
//   shader.setInt("skyLUT", TextureUnits::SKY_LUT);
//
// Units must not overlap within the same draw call.
namespace TextureUnits {
    constexpr int WATER_REFLECT = 0;
    constexpr int WATER_REFRACT = 1;
    constexpr int WATER_DUDV    = 2;
    constexpr int WATER_NORMAL  = 3;
    constexpr int WATER_DEPTH   = 4;
    constexpr int SSAO          = 5;
    constexpr int CSM_SHADOW    = 7;
    constexpr int CLOUDS        = 8;
    constexpr int SKY_LUT       = 9;
    constexpr int SCENE_COLOR   = 10;
    constexpr int SCENE_DEPTH   = 11;
    constexpr int CAUSTICS      = 12;
}
