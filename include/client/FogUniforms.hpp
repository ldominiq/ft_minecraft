#pragma once

#include <glad/glad.h>
#include <glm/glm.hpp>
#include "Shader.hpp"
#include "TextureUnits.hpp"

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
