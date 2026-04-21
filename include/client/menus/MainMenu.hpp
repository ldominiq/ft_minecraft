#ifndef MAINMENU_HPP
#define MAINMENU_HPP

#include "Menu.hpp"
#include <functional>
#include <array>

class MainMenu : public Menu {
public:
	MainMenu(float width, float height, GLuint dirtTex);
	~MainMenu();

	void setButtonCallback(std::function<void(int)> cb) { onButtonClick = std::move(cb); }
	void handleMouseClick(double mouseX, double mouseY, int button, int action) override;
	void handleMouseMove(double mouseX, double mouseY) override;

private:
	void onRender() override;
	void build() override;

	void drawTitle();
	void drawSplashText();

	std::function<void(int)> onButtonClick;

	GLuint dirtTexture = 0;
	GLuint titleTexture = 0;
	float titleTexAspect = 6.0f;

	std::array<Button, 4> buttons;

	std::string splashText;
};

#endif
