#include "MultiplayerMenu.hpp"
#include <cmath>
#include <GLFW/glfw3.h>

static constexpr float DESIGN_W = 960.0f;
static constexpr float DESIGN_H = 540.0f;

static constexpr float BTN_W = 200.0f;
static constexpr float BTN_H = 40.0f;
static constexpr float INPUT_W = 400.0f;
static constexpr float INPUT_H = 40.0f;

MultiplayerMenu::MultiplayerMenu(float width, float height, GLuint dirtTex)
	: Menu(DESIGN_W, DESIGN_H), dirtTexture(dirtTex)
{
	connectButton.label = "Connect";
	cancelButton.label = "Cancel";

	resize(width, height);
}

void MultiplayerMenu::build()
{
	float centerX = fullscreenWidth / 2.0f;
	float centerY = fullscreenHeight / 2.0f;

	// Input box centered
	inputBoxW = INPUT_W * menuScale;
	inputBoxH = INPUT_H * menuScale;
	inputBoxX = centerX - inputBoxW / 2.0f;
	inputBoxY = centerY + 10.0f * menuScale;

	// Buttons below input
	float btnW = BTN_W * menuScale;
	float btnH = BTN_H * menuScale;
	float gap = 10.0f * menuScale;

	connectButton.w = btnW;
	connectButton.h = btnH;
	connectButton.x = centerX - btnW - gap / 2.0f;
	connectButton.y = centerY - btnH - 20.0f * menuScale;

	cancelButton.w = btnW;
	cancelButton.h = btnH;
	cancelButton.x = centerX + gap / 2.0f;
	cancelButton.y = centerY - btnH - 20.0f * menuScale;
}

void MultiplayerMenu::drawTiledBackground()
{
	float tileSize = 64.0f * menuScale;
	drawTiledTexturedQuad(0, 0, fullscreenWidth, fullscreenHeight, dirtTexture, tileSize);
	drawSimpleQuad(0, 0, fullscreenWidth, fullscreenHeight, glm::vec4(0.0f, 0.0f, 0.0f, 0.4f));
}

void MultiplayerMenu::addChar(char c)
{
	if (ipAddress.size() >= 45) return;
	if ((c >= '0' && c <= '9') || c == '.' || c == ':') {
		ipAddress += c;
		clearError();
	}
}

void MultiplayerMenu::removeChar()
{
	if (!ipAddress.empty()) {
		ipAddress.pop_back();
		clearError();
	}
}

void MultiplayerMenu::onRender()
{
	drawTiledBackground();

	// Title (high-resolution Typer for crisp rendering)
	float savedScale = textRenderer.getScale();
	titleRenderer.setScale(0.45f * menuScale);
	titleRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	std::string title = "Direct Connection";
	float titleWidth = titleRenderer.getPixelSizeOfString(title);
	float titleX = (fullscreenWidth - titleWidth) / 2.0f;
	float titleY = fullscreenHeight - 100.0f * menuScale;

	titleRenderer.renderText(title, titleX, titleY, glm::vec3(1.0f));

	// Label above input
	float labelScale = 0.45f * menuScale;
	textRenderer.setScale(labelScale);
	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);
	textRenderer.renderText("Server Address", inputBoxX, inputBoxY + inputBoxH + 10.0f * menuScale, glm::vec3(0.8f));

	// Input box border
	float border = 2.0f * menuScale;
	drawSimpleQuad(inputBoxX - border, inputBoxY - border,
				   inputBoxW + 2 * border, inputBoxH + 2 * border,
				   glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));

	// Input box background
	drawSimpleQuad(inputBoxX, inputBoxY, inputBoxW, inputBoxH,
				   glm::vec4(0.0f, 0.0f, 0.0f, 0.8f));

	// IP text inside box
	float textScale = 0.5f * menuScale;
	textRenderer.setScale(textScale);
	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	// Blinking cursor
	std::string displayText = ipAddress;
	bool showCursor = static_cast<int>(glfwGetTime() * 2.0f) % 2 == 0;
	if (showCursor) displayText += "_";

	textRenderer.renderText(displayText, inputBoxX + 8.0f * menuScale, inputBoxY + inputBoxH * 0.25f, glm::vec3(1.0f));

	drawButton(connectButton.x, connectButton.y, connectButton.w, connectButton.h,
			   connectButton.label, connectButton.hovered);
	drawButton(cancelButton.x, cancelButton.y, cancelButton.w, cancelButton.h,
			   cancelButton.label, cancelButton.hovered);

	// Error message (red, between input box and buttons)
	if (!errorMessage.empty()) {
		float errScale = 0.4f * menuScale;
		textRenderer.setScale(errScale);
		textRenderer.setProjection(fullscreenWidth, fullscreenHeight);
		float errWidth = textRenderer.getPixelSizeOfString(errorMessage);
		float errX = (fullscreenWidth - errWidth) / 2.0f;
		float errY = inputBoxY - 20.0f * menuScale;
		textRenderer.renderText(errorMessage, errX, errY, glm::vec3(1.0f, 0.3f, 0.3f));
	}

	textRenderer.setScale(savedScale);
}

void MultiplayerMenu::handleMouseMove(double mouseX, double mouseY)
{
	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	connectButton.hovered =
		glX >= connectButton.x && glX <= connectButton.x + connectButton.w &&
		glY >= connectButton.y && glY <= connectButton.y + connectButton.h;

	cancelButton.hovered =
		glX >= cancelButton.x && glX <= cancelButton.x + cancelButton.w &&
		glY >= cancelButton.y && glY <= cancelButton.y + cancelButton.h;
}

void MultiplayerMenu::handleMouseClick(double mouseX, double mouseY, int button, int action)
{
	if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
		return;

	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	if (glX >= connectButton.x && glX <= connectButton.x + connectButton.w &&
		glY >= connectButton.y && glY <= connectButton.y + connectButton.h) {
		if (onConnect) onConnect(ipAddress);
		return;
	}

	if (glX >= cancelButton.x && glX <= cancelButton.x + cancelButton.w &&
		glY >= cancelButton.y && glY <= cancelButton.y + cancelButton.h) {
		if (onCancel) onCancel();
		return;
	}
}
