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
	changeControlsButton.label = "Controls";
	resize(width, height);
	username = "nameless";
	loadUsername();
}

void SettingsMenu::addChar(char c)
{
	if (!usernameEditable) return;
	if (username.size() >= 16) return;

	username += c;
}

void SettingsMenu::removeChar()
{
	if (!usernameEditable) return;
	if (!username.empty()) {
		username.pop_back();
	}
}

void SettingsMenu::build()
{
	float centerX = fullscreenWidth / 2.0f;
	float quarterY = fullscreenHeight / 3.0f;

	float btnW = BTN_W * menuScale;
	float btnH = BTN_H * menuScale;

	nameBox.w = btnW;
	nameBox.h = btnH;
	nameBox.x = centerX - btnW / 2.0f;
	nameBox.y = fullscreenHeight - quarterY - nameBox.h;

	changeControlsButton.w = btnW;
	changeControlsButton.h = btnH;
	changeControlsButton.x = centerX - btnW / 2.0f;
	changeControlsButton.y = nameBox.y - btnH - 20.0f * menuScale;

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

	// playerName text
	float msgScale = 0.6f * menuScale;
	textRenderer.setScale(msgScale);
	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	if (usernameEditable)
		drawInputBox(nameBox.x, nameBox.y, nameBox.w, nameBox.h, username, true);

	drawButton(doneButton.x, doneButton.y, doneButton.w, doneButton.h,
			   doneButton.label, doneButton.hovered);
	
	drawButton(changeControlsButton.x, changeControlsButton.y, changeControlsButton.w, changeControlsButton.h,
			   changeControlsButton.label, changeControlsButton.hovered);

	textRenderer.setScale(savedScale);
}

void SettingsMenu::handleMouseMove(double mouseX, double mouseY)
{
	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	doneButton.hovered =
		glX >= doneButton.x && glX <= doneButton.x + doneButton.w &&
		glY >= doneButton.y && glY <= doneButton.y + doneButton.h;
	
	changeControlsButton.hovered =
		glX >= changeControlsButton.x && glX <= changeControlsButton.x + changeControlsButton.w &&
		glY >= changeControlsButton.y && glY <= changeControlsButton.y + changeControlsButton.h;
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

	if (glX >= changeControlsButton.x && glX <= changeControlsButton.x + changeControlsButton.w &&
		glY >= changeControlsButton.y && glY <= changeControlsButton.y + changeControlsButton.h) {
		if (changeControls) changeControls();
	}
}

void SettingsMenu::saveUsername(const char* filename)
{
	std::ofstream file(filename);
	if (!file.is_open()) return;

	file << username;
}

void SettingsMenu::loadUsername(const char* filename)
{
	std::ifstream file(filename);
	if (!file.is_open()) return;

	std::getline(file, username);
}
