#ifndef SHADER_HPP
#define SHADER_HPP

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

#include "stb_image.h"

class Shader {
public:
    GLuint ID;

    Shader(const char* vertexPath, const char* fragmentPath);
    void use() const;

    void stop() const;

    void setBool(const std::string& name, bool value) const;
    void setInt(const std::string& name, int value) const;
    void setFloat(const std::string& name, float value) const;
    void setFloat3(const std::string& name, const float &v1, const float &v2, const float &v3) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;
    void setVec2(const std::string &name, const glm::vec2& vec2) const;
    void setVec3(const std::string& name, const glm::vec3& vec3) const;
    void setVec4(const std::string& name, const glm::vec4& vec4) const;

    unsigned int loadTexture(const char* path);

protected:
    int getUniformLocation(const std::string& name) const;
    void loadMatrix(int location, const glm::mat4& matrix) const;
    void bindAttribute(int attribute, const std::string &name);
    void bindAttributes();

    void getAllUniformLocations(const glm::mat4& matrix);

private:
    int location_transformationMatrix = -1;;
};

#endif