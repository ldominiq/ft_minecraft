
#include "Menu.hpp"

Menu::Menu(float width, float height)
: DESIGN_WIDTH(width), DESIGN_HEIGHT(height), textRenderer("fonts/Roboto-Regular.ttf")
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
	
		glDeleteTextures(1, &textureVAO);
		glDeleteTextures(1, &textureVBO);
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
