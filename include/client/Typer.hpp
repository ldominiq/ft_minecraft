
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

#ifdef _WIN32
typedef unsigned int uint;
#endif

struct TypingCharacter {
    unsigned int TextureID;  // ID handle of the glyph texture
    glm::ivec2   Size;       // Size of glyph
    glm::ivec2   Bearing;    // Offset from baseline to left/top of glyph
    unsigned int Advance;    // Offset to advance to next glyph
};

class Typer {
private:
	float scale = 0.3;
	Shader shader;
	std::map<GLchar, TypingCharacter> Characters;
	unsigned int VAO, VBO;

public:
    Typer(const std::string& fontPath, unsigned int pixelSize = 48);
    ~Typer();

	uint getPixelSizeOfString(const std::string &str);
	float getAscent(); // pixel ascent at current scale (height of 'A')
	void setProjection(int width, int height);
	void setScale (float scale) {this->scale = scale;}
	float getScale() const { return scale; }
	void renderText(const std::string &text, float x, float y, const glm::vec3 &color, const float alpha = 1.0f, float rotationDeg = 0.0f);
};

#endif
