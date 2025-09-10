#ifndef SHADER_HPP
#define SHADER_HPP

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

class Shader {
public:
    GLuint ID;

    Shader(const char* vertexPath, const char* fragmentPath);
    void use() const;

    void setInt(const std::string& name, int value) const;
	void setFloat3(const std::string& name, const float &v1, const float &v2, const float &v3) const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;
    void setVec3(const std::string& name, const glm::vec3& vec3) const;
	void setVec4(const std::string& name, const glm::vec4& vec4) const;
};

#endif