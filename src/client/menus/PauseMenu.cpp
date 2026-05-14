
#include "PauseMenu.hpp"

PauseMenu::PauseMenu(float width, float height): Menu(width, height)
{
	build();
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

	backToMainMenuButton.w = btnW;
	backToMainMenuButton.h = btnH;
	backToMainMenuButton.x = centerX - btnW / 2.0f;
	backToMainMenuButton.y = continueButton.y - btnH - 20.0f * menuScale;
	backToMainMenuButton.label = "Main Menu";
}

void PauseMenu::handleMouseClick(double mouseX, double mouseY, int button, int action)
{
	if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
		return;

	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	if (glX >= continueButton.x && glX <= continueButton.x + continueButton.w &&
		glY >= continueButton.y && glY <= continueButton.y + continueButton.h) {
		if (onContinue) onContinue();
	}

	if (glX >= backToMainMenuButton.x && glX <= backToMainMenuButton.x + backToMainMenuButton.w &&
		glY >= backToMainMenuButton.y && glY <= backToMainMenuButton.y + backToMainMenuButton.h) {
		if (onBackToMainMenu) onBackToMainMenu();
	}

}

void PauseMenu::handleMouseMove(double mouseX, double mouseY)
{
	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	for (auto& button : {&continueButton, &backToMainMenuButton})
	{
		button->hovered = glX >= button->x && glX <= button->x + button->w &&
						  glY >= button->y && glY <= button->y + button->h;
	}
}

void PauseMenu::onRender()
{
	drawSimpleQuad(0, 0, fullscreenWidth, fullscreenHeight, glm::vec4(0.0f, 0.0f, 0.0f, 0.5f));
	drawButton(continueButton.x, continueButton.y, continueButton.w, continueButton.h, continueButton.label, continueButton.hovered);
	drawButton(backToMainMenuButton.x, backToMainMenuButton.y, backToMainMenuButton.w, backToMainMenuButton.h, backToMainMenuButton.label, backToMainMenuButton.hovered);
}
