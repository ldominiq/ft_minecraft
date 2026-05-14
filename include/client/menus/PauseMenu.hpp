
#ifndef PAUSEMENU_HPP
#define PAUSEMENU_HPP

#include "Menu.hpp"
#include <functional>

static constexpr float DESIGN_W = 960.0f;
static constexpr float DESIGN_H = 540.0f;

static constexpr float BTN_W = 200.0f;
static constexpr float BTN_H = 40.0f;

class PauseMenu : public Menu
{
public:
	PauseMenu(float width, float height);
	~PauseMenu();
	
	void setContinueCallback(std::function<void()> cb) { onContinue = std::move(cb); }
	void setBackToMainMenuCallback(std::function<void()> cb) { onBackToMainMenu = std::move(cb); }

	void handleMouseClick(double mouseX, double mouseY, int button, int action) override;
	void handleMouseMove(double mouseX, double mouseY) override;

private:

	Button continueButton;
	Button backToMainMenuButton;

	std::function<void()> onContinue;
	std::function<void()> onBackToMainMenu;

	void onRender() override;
	void build() override;
};

#endif // PAUSEMENU_HPP