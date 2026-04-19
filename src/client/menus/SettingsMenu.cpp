#include "SettingsMenu.hpp"
#include <GLFW/glfw3.h>

static constexpr float DESIGN_W = 960.0f;
static constexpr float DESIGN_H = 540.0f;

static constexpr float BTN_W = 200.0f;
static constexpr float BTN_H = 40.0f;

SettingsMenu::SettingsMenu(float width, float height, GLuint dirtTex)
	: Menu(DESIGN_W, DESIGN_H), dirtTexture(dirtTex)
{
	doneButton.label = "Done";
	resize(width, height);
}

void SettingsMenu::build()
{
	float centerX = fullscreenWidth / 2.0f;

	float btnW = BTN_W * menuScale;
	float btnH = BTN_H * menuScale;

	doneButton.w = btnW;
	doneButton.h = btnH;
	doneButton.x = centerX - btnW / 2.0f;
	doneButton.y = 60.0f * menuScale;
}

void SettingsMenu::drawTiledBackground()
{
	float tileSize = 64.0f * menuScale;
	drawTiledTexturedQuad(0, 0, fullscreenWidth, fullscreenHeight, dirtTexture, tileSize);
	drawSimpleQuad(0, 0, fullscreenWidth, fullscreenHeight, glm::vec4(0.0f, 0.0f, 0.0f, 0.4f));
}

void SettingsMenu::onRender()
{
	drawTiledBackground();

	float savedScale = textRenderer.getScale();

	// Title (high-resolution Typer for crisp rendering)
	titleRenderer.setScale(0.45f * menuScale);
	titleRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	std::string title = "Settings";
	float titleWidth = titleRenderer.getPixelSizeOfString(title);
	float titleX = (fullscreenWidth - titleWidth) / 2.0f;
	float titleY = fullscreenHeight - 100.0f * menuScale;

	titleRenderer.renderText(title, titleX, titleY, glm::vec3(1.0f));

	// Coming soon text
	float msgScale = 0.6f * menuScale;
	textRenderer.setScale(msgScale);
	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	std::string msg = "Coming soon...";
	float msgWidth = textRenderer.getPixelSizeOfString(msg);
	float msgX = (fullscreenWidth - msgWidth) / 2.0f;
	float msgY = fullscreenHeight / 2.0f;

	textRenderer.renderText(msg, msgX, msgY, glm::vec3(0.7f));

	// Done button
	float b = 2.0f * menuScale;
	glm::vec4 brdColor = doneButton.hovered
		? glm::vec4(0.7f, 0.7f, 0.8f, 1.0f)
		: glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
	glm::vec4 bgColor = doneButton.hovered
		? glm::vec4(0.4f, 0.4f, 0.5f, 0.9f)
		: glm::vec4(0.25f, 0.25f, 0.3f, 0.85f);

	drawSimpleQuad(doneButton.x - b, doneButton.y - b,
				   doneButton.w + 2 * b, doneButton.h + 2 * b, brdColor);
	drawSimpleQuad(doneButton.x, doneButton.y, doneButton.w, doneButton.h, bgColor);

	float lblScale = 0.5f * menuScale;
	textRenderer.setScale(lblScale);
	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);
	float lblWidth = textRenderer.getPixelSizeOfString(doneButton.label);
	float ascent = textRenderer.getAscent();
	float lblX = doneButton.x + (doneButton.w - lblWidth) / 2.0f;
	float lblY = doneButton.y + (doneButton.h - ascent) / 2.0f;
	textRenderer.renderText(doneButton.label, lblX, lblY, glm::vec3(1.0f));

	textRenderer.setScale(savedScale);
}

void SettingsMenu::handleMouseMove(double mouseX, double mouseY)
{
	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	doneButton.hovered =
		glX >= doneButton.x && glX <= doneButton.x + doneButton.w &&
		glY >= doneButton.y && glY <= doneButton.y + doneButton.h;
}

void SettingsMenu::handleMouseClick(double mouseX, double mouseY, int button, int action)
{
	if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
		return;

	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	if (glX >= doneButton.x && glX <= doneButton.x + doneButton.w &&
		glY >= doneButton.y && glY <= doneButton.y + doneButton.h) {
		if (onDone) onDone();
	}
}
