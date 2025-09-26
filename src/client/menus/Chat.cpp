
#include "Chat.hpp"

Chat::Chat(float width, float height) : Menu(width, height), textRenderer("fonts/Roboto-Regular.ttf", scale) {

	x = 0.0;
	y = 0.0f;
	w = width / 3.0f;
	h = height / 3.0f;
	color = glm::vec4(0,0,0,0.6f);
	chatLog.push_back(std::string());
	textRenderer.setProjection(width, height);

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 6 * 2, nullptr, GL_DYNAMIC_DRAW); //STATIC_DRAW?

	glEnableVertexAttribArray(0);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
}

Chat::~Chat() {
	if (glfwGetCurrentContext()) {
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
	} else {
		VAO = 0;
		VBO = 0;
	}
}

void Chat::onRender()
{
	drawSimpleQuad(x, y, w, h);
	float offset = 10.0f;
	float charHeight = (48+10)*scale; //48 cause font is 48 and 10 is height offset

	// textRenderer.renderText(currMsg, x+10, y+10, glm::vec3(1.0f));
	textRenderer.renderText(currMsg, offset, offset, glm::vec3(0.5, 0.8f, 0.2f));

	int currHeight = offset + charHeight;
    for (auto it = chatLog.rbegin(); it != chatLog.rend() && currHeight < (h - charHeight); ++it) {
		
		textRenderer.renderText(it->c_str(), x+offset, y+currHeight, glm::vec3(1.0f));
		currHeight += charHeight;
    }
}

void Chat::addCharToCurrMsg(const char &c)
{
	if (textRenderer.getPixelSizeOfString(currMsg) + textRenderer.getPixelSizeOfString("0") > (w - 10.0f)) //offset 1 more character.
		return ;
	currMsg.push_back(c);
}

void Chat::removeCharFromCurrMsg()
{
	if (currMsg.empty()) return ;
	currMsg.pop_back();
}

void Chat::updateChatlog(const std::string &str)
{
	chatLog.push_back(str);
}