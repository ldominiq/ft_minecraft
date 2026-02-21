
#include "Menu.hpp"

Menu::Menu(float width, float height) : width(width), height(height) {
	simpleQuadShader = std::make_unique<Shader>("shaders/chat.vert", "shaders/chat.frag");

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 2, nullptr, GL_STATIC_DRAW);

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
}

Menu::~Menu()
{
	if (glfwGetCurrentContext()) {
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
	} else {
		VAO = 0;
		VBO = 0;
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
        0.0f, static_cast<float>(width),
        0.0f, static_cast<float>(height),
        -1.0f, 1.0f
    );
	simpleQuadShader->setMat4("uProjection", proj);

    glDrawArrays(GL_TRIANGLES, 0, 6);
}

void Menu::render() {
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	onRender();

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
}
