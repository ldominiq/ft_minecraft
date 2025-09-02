
#include "Menu.hpp"

// void Menu::drawText(float x, float y, const std::string& text) {
//     float penX = x;
//     for (char c : text) {
//         int ascii = static_cast<unsigned char>(c);
//         int tx = ascii % 16; // col in atlas
//         int ty = ascii / 16; // row in atlas
//         float u0 = tx / 16.0f;
//         float v0 = ty / 16.0f;
//         float u1 = (tx+1) / 16.0f;
//         float v1 = (ty+1) / 16.0f;

//         drawQuad(penX, y, glyphW, glyphH, fontTextureID, u0,v0,u1,v1);
//         penX += glyphW;
//     }
// }

// Menu.cpp

Menu::Menu(float width, float height) : width(width), height(height) {
	simpleQuadShader = std::make_unique<Shader>("shaders/chat.vert", "shaders/chat.frag");
}

//no texture quad
void Menu::drawSimpleQuad(float x, float y, float w, float h) 
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
    glUniform4fv(glGetUniformLocation(simpleQuadShader->ID, "uColor"), 1, glm::value_ptr(color));

    glm::mat4 proj = glm::ortho(
        0.0f, static_cast<float>(width),
        0.0f, static_cast<float>(height),
        -1.0f, 1.0f
    );
    glUniformMatrix4fv(glGetUniformLocation(simpleQuadShader->ID, "uProjection"), 1, GL_FALSE, glm::value_ptr(proj));

    glBindVertexArray(VAO);
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
