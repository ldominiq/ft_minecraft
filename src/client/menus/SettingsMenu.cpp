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

void SettingsMenu::onRender()
{
	drawTiledBackground(dirtTexture);

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

	drawButton(doneButton.x, doneButton.y, doneButton.w, doneButton.h,
			   doneButton.label, doneButton.hovered);

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
