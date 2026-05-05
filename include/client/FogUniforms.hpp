#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "Shader.hpp"
#include "TextureUnits.hpp"

// Sun-direction Mie in-scatter parameters for distance fog.
// mieStrength=0 collapses to the legacy sky-LUT-only fog.
struct FogMieParams {
    float g        = 0.7f;
    float strength = 0.0f;
};

// Process-wide defaults; tweaked from the ImGui debug graphics tab.
inline FogMieParams& fogMieParams() {
    static FogMieParams p;
    return p;
}

// Upload distance-fog uniforms to a shader.
// Fog is disabled (fogEnabled=0) when active==false or skyLUTTex==0.
inline void uploadFogUniforms(Shader& shader, bool active, GLuint skyLUTTex,
                               float skyExposure, float fogStart, float fogEnd, float fogStrength)
{
    const bool reallyActive = active && skyLUTTex != 0;
    shader.setInt("fogEnabled", reallyActive ? 1 : 0);
    if (!reallyActive) return;

    glActiveTexture(GL_TEXTURE0 + TextureUnits::SKY_LUT);
    glBindTexture(GL_TEXTURE_2D, skyLUTTex);
    shader.setInt("skyLUT",       TextureUnits::SKY_LUT);
    shader.setFloat("skyExposure", skyExposure);
    shader.setFloat("fogStart",    fogStart);
    shader.setFloat("fogEnd",      fogEnd);
    shader.setFloat("fogStrength", fogStrength);
    const FogMieParams& mie = fogMieParams();
    shader.setFloat("fogMieG",        mie.g);
    shader.setFloat("fogMieStrength", mie.strength);
}

// Overload for shaders that also need an explicit sunDir uniform (e.g. water.frag).
inline void uploadFogUniforms(Shader& shader, bool active, GLuint skyLUTTex,
                               float skyExposure, float fogStart, float fogEnd, float fogStrength,
                               const glm::vec3& sunDir)
{
    uploadFogUniforms(shader, active, skyLUTTex, skyExposure, fogStart, fogEnd, fogStrength);
    if (active && skyLUTTex != 0)
        shader.setVec3("sunDir", sunDir);
}
