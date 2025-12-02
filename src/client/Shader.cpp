#include "Shader.hpp"
#include <unistd.h> // getcwd

Shader::Shader(const char* vertexPath, const char* fragmentPath) {
    std::string vCode;
    std::string fCode;
    int success;
    char infoLog[512];

    // Robust file loading without throwing iostream exceptions (avoids std::__ios_failure)
    {
        std::ifstream vFile(vertexPath, std::ios::in | std::ios::binary);
        if (!vFile.is_open()) {
            std::cerr << "ERROR::SHADER::VERTEX_FILE_OPEN_FAILED: '" << vertexPath << "'\n";
        } else {
            std::stringstream vStream;
            vStream << vFile.rdbuf();
            vCode = vStream.str();
        }
    }

    {
        std::ifstream fFile(fragmentPath, std::ios::in | std::ios::binary);
        if (!fFile.is_open()) {
            std::cerr << "ERROR::SHADER::FRAGMENT_FILE_OPEN_FAILED: '" << fragmentPath << "'\n";
        } else {
            std::stringstream fStream;
            fStream << fFile.rdbuf();
            fCode = fStream.str();
        }
    }

    if (vCode.empty() || fCode.empty()) {
        // Provide extra diagnostics about current working directory to help locate path issues
        char cwdBuf[1024] = {0};
        if (getcwd(cwdBuf, sizeof(cwdBuf) - 1)) {
            std::cerr << "ERROR::SHADER::EMPTY_SOURCE after read. CWD='" << cwdBuf << "'\n";
        }
    }
    const char* vShaderCode = vCode.c_str();
    const char* fShaderCode = fCode.c_str();

    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, nullptr);
    glCompileShader(vertex);
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, nullptr);
    glCompileShader(fragment);
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << std::endl;
    }

    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    glGetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(ID, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << std::endl;
    }

    glDeleteShader(vertex);
    glDeleteShader(fragment);
}

void Shader::use() const {
    glUseProgram(ID);
}

void Shader::stop() const {
    glUseProgram(0);
}

void Shader::setInt(const std::string& name, int value) const {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
}

void Shader::setMat4(const std::string& name, const glm::mat4& mat) const {
    glUniformMatrix4fv(glGetUniformLocation(ID, name.c_str()), 1, GL_FALSE, glm::value_ptr(mat));
}

void Shader::setVec2(const std::string &name, const glm::vec2& vec2) const {
    glUniform2fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(vec2));
} 

void Shader::setVec3(const std::string& name, const glm::vec3& vec3) const {
    glUniform3fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(vec3));
}

void Shader::setVec4(const std::string& name, const glm::vec4& vec4) const {
    glUniform4fv(glGetUniformLocation(ID, name.c_str()), 1, glm::value_ptr(vec4));
}

void Shader::setFloat(const std::string &name, float value) const {
    glUniform1f(glGetUniformLocation(ID, name.c_str()), value); 
} 

void Shader::setFloat3(const std::string& name, const float &v1, const float &v2, const float &v3) const {
    glUniform3f(glGetUniformLocation(ID, name.c_str()), v1, v2, v3);
}

int Shader::getUniformLocation(const std::string &name) const {
    return glGetUniformLocation(ID, name.c_str());
}

void Shader::loadMatrix(const int location, const glm::mat4& matrix) const {
    glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
}

void Shader::bindAttribute(int attribute, const std::string& name) {
    glBindAttribLocation(ID, attribute, name.c_str());
}

void Shader::bindAttributes() {
    bindAttribute(0, "position");
}

void Shader::getAllUniformLocations(const glm::mat4& matrix) {
    loadMatrix(location_transformationMatrix, matrix);
}

unsigned int Shader::loadTexture(const char* path) {
	GLuint texID;
	glGenTextures(1, &texID);
	glBindTexture(GL_TEXTURE_2D, texID);

	int w, h, ch;
	stbi_set_flip_vertically_on_load(true);
	unsigned char* data = stbi_load(path, &w, &h, &ch, 0);
	if (data) {
		const GLenum format = ch == 4 ? GL_RGBA : GL_RGB;
		glTexImage2D(GL_TEXTURE_2D, 0, format, w, h, 0, format, GL_UNSIGNED_BYTE, data);
		glGenerateMipmap(GL_TEXTURE_2D);
	} else {
		std::cerr << "Failed to load texture: " << path << "\n";
	}

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

	stbi_image_free(data);

	return texID;
}
