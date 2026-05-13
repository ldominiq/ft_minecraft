
#include "Menu.hpp"

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
	float textScale = 0.5f * menuScale;
	textRenderer.setScale(textScale);
	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	// Blinking cursor
	std::string displayText = text;
	bool showCursor = static_cast<int>(glfwGetTime() * 2.0f) % 2 == 0;
	if (showCursor && focused) displayText += "_";

	textRenderer.renderText(displayText, x + 8.0f * menuScale, y + h * 0.25f, glm::vec3(1.0f));
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

	textRenderer.setScale(textScale * scale);

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
