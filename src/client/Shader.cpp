#include "Shader.hpp"
#include <unordered_set>
#ifdef _WIN32
#include <direct.h>
#define getcwd _getcwd
#else
#include <unistd.h> // getcwd
#endif

static std::string resolveIncludes(
    const std::string& src,
    const std::filesystem::path& shaderDir,
    const std::filesystem::path& shaderRoot,
    std::unordered_set<std::string>& visited)
{
    std::string result;
    std::istringstream stream(src);
    std::string line;

    while (std::getline(stream, line)) {
        // Strip Windows CR if present
        if (!line.empty() && line.back() == '\r')
            line.pop_back();

        // Skip leading whitespace for include detection
        size_t first = line.find_first_not_of(" \t");
        if (first != std::string::npos && line.compare(first, 10, "#include \"") == 0) {
            size_t openQuote = line.find('"', first);
            size_t closeQuote = (openQuote == std::string::npos) ? std::string::npos : line.find('"', openQuote + 1);

            if (openQuote != std::string::npos && closeQuote != std::string::npos && closeQuote > openQuote + 1) {
                std::string rel = line.substr(openQuote + 1, closeQuote - openQuote - 1);

                // Reject absolute include paths — they would bypass shaderDir entirely
                if (std::filesystem::path(rel).is_absolute()) {
                    std::cerr << "ERROR::SHADER::INCLUDE_ABSOLUTE_PATH: " << rel << "\n";
                    result += "// REJECTED INCLUDE (absolute path): " + rel + '\n';
                    continue;
                }

                // Resolve and normalise without touching the filesystem
                std::filesystem::path includePath = (shaderDir / rel).lexically_normal();

                // Reject paths that escape the shader root via .. segments
                auto relToRoot = includePath.lexically_relative(shaderRoot);
                std::string relStr = relToRoot.string();
                if (relStr.empty() || relStr.rfind("..", 0) == 0) {
                    std::cerr << "ERROR::SHADER::INCLUDE_PATH_ESCAPE: " << includePath << "\n";
                    result += "// REJECTED INCLUDE (path escape): " + rel + '\n';
                    continue;
                }

                // Cycle detection — skip files already on the current include stack
                std::string normalStr = includePath.string();
                if (visited.count(normalStr)) {
                    std::cerr << "ERROR::SHADER::INCLUDE_CYCLE: " << includePath << "\n";
                    result += "// SKIPPED INCLUDE (cycle): " + rel + '\n';
                    continue;
                }

                std::ifstream inclFile(includePath, std::ios::in | std::ios::binary);
                if (!inclFile.is_open()) {
                    std::cerr << "ERROR::SHADER::INCLUDE_NOT_FOUND: " << includePath << "\n";
                    result += "// MISSING INCLUDE: " + rel + '\n';
                }
                else {
                    std::stringstream s;
                    s << inclFile.rdbuf();
                    visited.insert(normalStr);
                    result += resolveIncludes(s.str(), includePath.parent_path(), shaderRoot, visited) + '\n';
                    visited.erase(normalStr); // allow re-inclusion from other non-cyclic paths
                }
                continue;
            }
        }

        result += line + '\n';
    }

    return result;
}

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

    std::filesystem::path vRoot = std::filesystem::path(vertexPath).parent_path().lexically_normal();
    std::filesystem::path fRoot = std::filesystem::path(fragmentPath).parent_path().lexically_normal();
    std::unordered_set<std::string> vVisited, fVisited;
    vCode = resolveIncludes(vCode, vRoot, vRoot, vVisited);
    fCode = resolveIncludes(fCode, fRoot, fRoot, fVisited);

    const char* vShaderCode = vCode.c_str();
    const char* fShaderCode = fCode.c_str();

    GLuint vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, nullptr);
    glCompileShader(vertex);
    glGetShaderiv(vertex, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertex, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::VERTEX::COMPILATION_FAILED [" << vertexPath << "]\n" << infoLog << std::endl;
    }

    GLuint fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, nullptr);
    glCompileShader(fragment);
    glGetShaderiv(fragment, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragment, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED [" << fragmentPath << "]\n" << infoLog << std::endl;
    }

    ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    glGetProgramiv(ID, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(ID, 512, nullptr, infoLog);
        std::cerr << "ERROR::SHADER::PROGRAM::LINKING_FAILED [" << vertexPath << "][" << fragmentPath << "]\n" << infoLog << std::endl;
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

void Shader::setBool(const std::string& name, bool value) const {
    glUniform1i(glGetUniformLocation(ID, name.c_str()), value);
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
