
#include "PauseMenu.hpp"

static constexpr float DESIGN_W = 960.0f;
static constexpr float DESIGN_H = 540.0f;

static constexpr float BTN_W = 200.0f;
static constexpr float BTN_H = 40.0f;

PauseMenu::PauseMenu(float width, float height):  Menu(DESIGN_W, DESIGN_H)
{
	build();
	resize(width, height);
}

PauseMenu::~PauseMenu() {}

void PauseMenu::build()
{
	float centerX = fullscreenWidth / 2.0f;
	float halfY = fullscreenHeight * 3 / 7.0f;

	float btnW = BTN_W * menuScale;
	float btnH = BTN_H * menuScale;

	continueButton.w = btnW;
	continueButton.h = btnH;
	continueButton.x = centerX - btnW / 2.0f;
	continueButton.y = fullscreenHeight - halfY;
	continueButton.label = "Continue";

	settingsButton.w = btnW;
	settingsButton.h = btnH;
	settingsButton.x = centerX - btnW / 2.0f;
	settingsButton.y = continueButton.y - btnH - 20.0f * menuScale;
	settingsButton.label = "Settings";

	backToMainMenuButton.w = btnW;
	backToMainMenuButton.h = btnH;
	backToMainMenuButton.x = centerX - btnW / 2.0f;
	backToMainMenuButton.y = settingsButton.y - btnH - 20.0f * menuScale;
	backToMainMenuButton.label = "Main Menu";
}

void PauseMenu::handleMouseClick(double mouseX, double mouseY, int button, int action)
{
	if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
		return;

	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	if (isInside(continueButton, glX, glY) && onContinue) onContinue();
	if (isInside(settingsButton, glX, glY) && onSettings) onSettings();
	if (isInside(backToMainMenuButton, glX, glY) && onBackToMainMenu) onBackToMainMenu();
}

void PauseMenu::handleMouseMove(double mouseX, double mouseY)
{
	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	for (auto& button : {&continueButton, &settingsButton, &backToMainMenuButton})
		updateHover(*button, glX, glY);
}

void PauseMenu::onRender()
{
	drawSimpleQuad(0, 0, fullscreenWidth, fullscreenHeight, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
	drawButton(continueButton.x, continueButton.y, continueButton.w, continueButton.h, continueButton.label, continueButton.hovered);
	drawButton(settingsButton.x, settingsButton.y, settingsButton.w, settingsButton.h, settingsButton.label, settingsButton.hovered);
	drawButton(backToMainMenuButton.x, backToMainMenuButton.y, backToMainMenuButton.w, backToMainMenuButton.h, backToMainMenuButton.label, backToMainMenuButton.hovered);
}
