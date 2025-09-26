
#ifndef TYPER_HPP
#define TYPER_HPP

#include <ft2build.h>
#include FT_FREETYPE_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <map>
#include <string>

#include "Shader.hpp"
#include "GLFW/glfw3.h"


struct Character {
    unsigned int TextureID;  // ID handle of the glyph texture
    glm::ivec2   Size;       // Size of glyph
    glm::ivec2   Bearing;    // Offset from baseline to left/top of glyph
    unsigned int Advance;    // Offset to advance to next glyph
};

class Typer {
private:
	float scale;
	Shader shader;
	std::map<GLchar, Character> Characters;
	unsigned int VAO, VBO;

public:
    Typer(const std::string& fontPath, float scale);
    ~Typer();

	uint getPixelSizeOfString(const std::string &str);
	void setProjection(int width, int height);
	void renderText(const std::string &text, float x, float y, const glm::vec3 &color);
};

#endif
