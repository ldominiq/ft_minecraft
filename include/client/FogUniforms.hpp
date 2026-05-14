#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "Shader.hpp"
#include "TextureUnits.hpp"

// Upload distance-fog uniforms to a shader.
// Fog is disabled (fogEnabled=0) when active==false or skyLUTTex==0.
// hdrMode: when true, the fragment shader keeps fog as linear HDR radiance
// (the final tonemap happens in the cloud composite). When false, the legacy
// per-shader tonemapped fog is used.
inline void uploadFogUniforms(Shader& shader, bool active, GLuint skyLUTTex,
                               float skyExposure, float fogStart, float fogEnd, float fogStrength,
                               bool hdrMode = false)
{
    const bool reallyActive = active && skyLUTTex != 0;
    shader.setInt("fogEnabled", reallyActive ? 1 : 0);
    shader.setBool("hdrMode",  hdrMode);
    if (!reallyActive) return;

    glActiveTexture(GL_TEXTURE0 + TextureUnits::SKY_LUT);
    glBindTexture(GL_TEXTURE_2D, skyLUTTex);
    shader.setInt("skyLUT",       TextureUnits::SKY_LUT);
    shader.setFloat("skyExposure", skyExposure);
    shader.setFloat("fogStart",    fogStart);
    shader.setFloat("fogEnd",      fogEnd);
    shader.setFloat("fogStrength", fogStrength);
}

// Overload for shaders that also need an explicit sunDir uniform (e.g. water.frag).
inline void uploadFogUniforms(Shader& shader, bool active, GLuint skyLUTTex,
                               float skyExposure, float fogStart, float fogEnd, float fogStrength,
                               const glm::vec3& sunDir, bool hdrMode = false)
{
    uploadFogUniforms(shader, active, skyLUTTex, skyExposure, fogStart, fogEnd, fogStrength, hdrMode);
    if (active && skyLUTTex != 0)
        shader.setVec3("sunDir", sunDir);
}
