
#include "Menu.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

Menu::Menu(float width, float height)
: DESIGN_WIDTH(width), DESIGN_HEIGHT(height),
  textRenderer("fonts/upheavtt.ttf"),
  titleRenderer("fonts/upheavtt.ttf", 128)
{
    resize(width, height);

    simpleQuadShader = std::make_unique<Shader>("shaders/chat.vert", "shaders/chat.frag");
    texturedQuadShader = std::make_unique<Shader>("shaders/textureQuad.vert", "shaders/textureQuad.frag");

    // ---- SIMPLE QUAD ----
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 2, nullptr, GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);


    // ---- TEXTURED QUAD ----
    glGenVertexArrays(1, &textureVAO);
    glGenBuffers(1, &textureVBO);

    glBindVertexArray(textureVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textureVBO);

    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 4, nullptr, GL_DYNAMIC_DRAW);

    // position
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // uv
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

Menu::~Menu()
{
	if (glfwGetCurrentContext()) {
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);

		glDeleteVertexArrays(1, &textureVAO);
		glDeleteBuffers(1, &textureVBO);
	} else {
		VAO = 0;
		VBO = 0;

		textureVAO = 0;
		textureVBO = 0;
	}
}

//no texture quad
void Menu::drawSimpleQuad(float x, float y, float w, float h, const glm::vec4 &color) const
{
	float verts[] = {
		x, y,
		x+w, y,
		x+w, y+h,

		x, y,
		x+w, y+h,
		x, y+h
	};

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    simpleQuadShader->use();
	simpleQuadShader->setVec4("uColor", color);

    glm::mat4 proj = glm::ortho(
        0.0f, static_cast<float>(fullscreenWidth),
        0.0f, static_cast<float>(fullscreenHeight),
        -1.0f, 1.0f
    );
	simpleQuadShader->setMat4("uProjection", proj);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Menu::drawTexturedQuad(float x, float y, float w, float h, unsigned int textureID, float alpha)
{
    // Vertex positions + UVs
    float verts[] = {
        // x, y,   u, v
        x,     y,     0.0f, 0.0f,
        x + w, y,     1.0f, 0.0f,
        x + w, y + h, 1.0f, 1.0f,

        x,     y,     0.0f, 0.0f,
        x + w, y + h, 1.0f, 1.0f,
        x,     y + h, 0.0f, 1.0f
    };

    glBindVertexArray(textureVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textureVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    texturedQuadShader->use();
    texturedQuadShader->setMat4("uProjection", glm::ortho(0.0f, static_cast<float>(fullscreenWidth),
                                                         0.0f, static_cast<float>(fullscreenHeight),
                                                         -1.0f, 1.0f));
    texturedQuadShader->setFloat("uAlpha", alpha);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    texturedQuadShader->setInt("uTexture", 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Menu::drawTiledTexturedQuad(float x, float y, float w, float h, unsigned int textureID, float tileSize)
{
    float tilesX = w / tileSize;
    float tilesY = h / tileSize;

    float verts[] = {
        x,     y,     0.0f,   0.0f,
        x + w, y,     tilesX, 0.0f,
        x + w, y + h, tilesX, tilesY,

        x,     y,     0.0f,   0.0f,
        x + w, y + h, tilesX, tilesY,
        x,     y + h, 0.0f,   tilesY
    };

    glBindVertexArray(textureVAO);
    glBindBuffer(GL_ARRAY_BUFFER, textureVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(verts), verts);

    texturedQuadShader->use();
    texturedQuadShader->setMat4("uProjection", glm::ortho(0.0f, static_cast<float>(fullscreenWidth),
                                                         0.0f, static_cast<float>(fullscreenHeight),
                                                         -1.0f, 1.0f));
    texturedQuadShader->setFloat("uAlpha", 1.0f);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    texturedQuadShader->setInt("uTexture", 0);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Menu::drawTiledBackground(GLuint &texture)
{
	float tileSize = 64.0f * menuScale;
	drawTiledTexturedQuad(0, 0, fullscreenWidth, fullscreenHeight, texture, tileSize);

	// Darken overlay
	drawSimpleQuad(0, 0, fullscreenWidth, fullscreenHeight, glm::vec4(0.0f, 0.0f, 0.0f, 0.4f));
}

void Menu::drawButton(float x, float y, float w, float h, const std::string& label, bool hovered, bool enabled)
{
	glm::vec4 bgColor;
	if (!enabled)
		bgColor = glm::vec4(0.2f, 0.2f, 0.2f, 0.8f);
	else if (hovered)
		bgColor = glm::vec4(0.4f, 0.4f, 0.5f, 0.9f);
	else
		bgColor = glm::vec4(0.25f, 0.25f, 0.3f, 0.85f);

	float border = 2.0f * menuScale;
	glm::vec4 borderColor = (hovered && enabled)
		? glm::vec4(0.7f, 0.7f, 0.8f, 1.0f)
		: glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);

	drawSimpleQuad(x - border, y - border, w + 2 * border, h + 2 * border, borderColor);
	drawSimpleQuad(x, y, w, h, bgColor);

	float savedScale = textRenderer.getScale();
	float labelScale = 0.5f * menuScale;
	textRenderer.setScale(labelScale);
	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	float labelWidth = textRenderer.getPixelSizeOfString(label);
	float ascent = textRenderer.getAscent();
	float labelX = x + (w - labelWidth) / 2.0f;
	float labelY = y + (h - ascent) / 2.0f;

	glm::vec3 textColor = enabled ? glm::vec3(1.0f) : glm::vec3(0.5f);
	textRenderer.renderText(label, labelX, labelY, textColor);
	textRenderer.setScale(savedScale);
}

void Menu::drawInputBox(float x, float y, float w, float h, const std::string& text, bool focused)
{
	// Input box border
	float border = 2.0f * menuScale;
	drawSimpleQuad(x - border, y - border,
				   w + 2 * border, h + 2 * border,
				   glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));

	// Input box background
	drawSimpleQuad(x, y, w, h,
				   glm::vec4(0.0f, 0.0f, 0.0f, 0.8f));

	// text inside box
	float savedScale = textRenderer.getScale();
	float baseScale = 0.5f * menuScale;
	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	// Blinking cursor
	std::string displayText = text;
	bool showCursor = static_cast<int>(glfwGetTime() * 2.0f) % 2 == 0;
	if (showCursor && focused) displayText += "_";

	// Auto-shrink so long text stays inside the box
	float padX = 8.0f * menuScale;
	float maxTextW = w - 2.0f * padX;
	float useScale = baseScale;
	textRenderer.setScale(useScale);
	float textW = static_cast<float>(textRenderer.getPixelSizeOfString(displayText));
	if (textW > maxTextW && textW > 0.0f) {
		useScale = baseScale * (maxTextW / textW);
		textRenderer.setScale(useScale);
	}

	textRenderer.renderText(displayText, x + padX, y + h * 0.25f, glm::vec3(1.0f));
	textRenderer.setScale(savedScale);
}

bool Menu::isInside(const Button& b, float glX, float glY)
{
	return glX >= b.x && glX <= b.x + b.w &&
		   glY >= b.y && glY <= b.y + b.h;
}

void Menu::updateHover(Button& b, float glX, float glY)
{
	b.hovered = isInside(b, glX, glY);
}

void Menu::drawToggle(const Toggle& t)
{
	std::string label = t.label + ": " + (t.get() ? "ON" : "OFF");
	drawButton(t.btn.x, t.btn.y, t.btn.w, t.btn.h, label, t.btn.hovered, t.btn.enabled);
}

bool Menu::clickToggle(Toggle& t, float glX, float glY)
{
	if (!isInside(t.btn, glX, glY)) return false;
	t.set(!t.get());
	return true;
}

void Menu::applySliderAtX(Slider& s, float glX)
{
	if (s.track.w <= 0.0f) return;
	float t = std::clamp((glX - s.track.x) / s.track.w, 0.0f, 1.0f);
	float v = s.minVal + t * (s.maxVal - s.minVal);
	if (s.isInt)
		s.setI(static_cast<int>(std::round(v)));
	else
		s.setF(v);
}

bool Menu::clickSlider(Slider& s, float glX, float glY)
{
	if (!isInside(s.track, glX, glY)) return false;
	s.dragging = true;
	applySliderAtX(s, glX);
	return true;
}

void Menu::dragSlider(Slider& s, float glX)
{
	if (!s.dragging) return;
	applySliderAtX(s, glX);
}

void Menu::addFloatSlider(const std::string& label, float min, float max,
						  std::function<float()> get,
						  std::function<void(float)> set,
						  int precision)
{
	Slider s;
	s.label = label;
	s.minVal = min;
	s.maxVal = max;
	s.isInt = false;
	s.precision = precision;
	s.getF = std::move(get);
	s.setF = std::move(set);
	sliders.push_back(std::move(s));
}

void Menu::addIntSlider(const std::string& label, int min, int max,
						std::function<int()> get,
						std::function<void(int)> set)
{
	Slider s;
	s.label = label;
	s.minVal = static_cast<float>(min);
	s.maxVal = static_cast<float>(max);
	s.isInt = true;
	s.getI = std::move(get);
	s.setI = std::move(set);
	sliders.push_back(std::move(s));
}

void Menu::drawSlider(const Slider& s)
{
	float border = 2.0f * menuScale;
	drawSimpleQuad(s.track.x - border, s.track.y - border,
				   s.track.w + 2 * border, s.track.h + 2 * border,
				   glm::vec4(0.4f, 0.4f, 0.4f, 1.0f));
	drawSimpleQuad(s.track.x, s.track.y, s.track.w, s.track.h,
				   glm::vec4(0.15f, 0.15f, 0.2f, 0.85f));

	float current = s.isInt ? static_cast<float>(s.getI()) : s.getF();
	float t = std::clamp((current - s.minVal) / std::max(1e-6f, (s.maxVal - s.minVal)),
						 0.0f, 1.0f);

	float fillW = t * s.track.w;
	drawSimpleQuad(s.track.x, s.track.y, fillW, s.track.h,
				   glm::vec4(0.35f, 0.45f, 0.7f, 0.9f));

	float knobW = 6.0f * menuScale;
	float knobX = s.track.x + fillW - knobW / 2.0f;
	knobX = std::clamp(knobX, s.track.x, s.track.x + s.track.w - knobW);
	drawSimpleQuad(knobX, s.track.y - 2.0f * menuScale,
				   knobW, s.track.h + 4.0f * menuScale,
				   glm::vec4(0.9f, 0.9f, 0.95f, 1.0f));

	float savedScale = textRenderer.getScale();
	textRenderer.setScale(0.4f * menuScale);
	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	std::ostringstream ss;
	if (s.isInt)
		ss << s.label << ": " << static_cast<int>(std::round(current));
	else
		ss << s.label << ": " << std::fixed << std::setprecision(s.precision) << current;
	std::string text = ss.str();

	float labelWidth = textRenderer.getPixelSizeOfString(text);
	float ascent = textRenderer.getAscent();
	float labelX = s.track.x + (s.track.w - labelWidth) / 2.0f;
	float labelY = s.track.y + (s.track.h - ascent) / 2.0f;
	textRenderer.renderText(text, labelX, labelY, glm::vec3(1.0f));
	textRenderer.setScale(savedScale);
}

GLuint Menu::loadTexture2D(const char* path, bool pixelated, int* outWidth, int* outHeight)
{
    int width, height, channels;
    unsigned char* data = stbi_load(path, &width, &height, &channels, 4);
    if (!data) {
        std::cerr << "Failed to load texture: " << path << std::endl;
        if (outWidth)  *outWidth = 0;
        if (outHeight) *outHeight = 0;
        return 0;
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    GLint filter = pixelated ? GL_NEAREST : GL_LINEAR;
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);

    stbi_image_free(data);
    if (outWidth)  *outWidth = width;
    if (outHeight) *outHeight = height;
    return texture;
}

void Menu::resize(float width, float height)
{
	fullscreenWidth = width;
	fullscreenHeight = height;

	float scaleX = width / DESIGN_WIDTH;
	float scaleY = height / DESIGN_HEIGHT;

	float scale = std::min(scaleX, scaleY);

	menuScale = scale;
	this->menuWidth = DESIGN_WIDTH  * scale;
	this->menuHeight = DESIGN_HEIGHT * scale;

	textRenderer.setScale(scale);

	std::cout << scale << std::endl;

	build();
}

void Menu::render() {
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	onRender();

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
}
