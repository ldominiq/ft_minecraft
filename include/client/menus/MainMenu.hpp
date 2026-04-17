#ifndef MAINMENU_HPP
#define MAINMENU_HPP

#include "Menu.hpp"
#include <functional>
#include <array>

class MainMenu : public Menu {
public:
	MainMenu(float width, float height);
	~MainMenu();

	void setButtonCallback(std::function<void(int)> cb) { onButtonClick = std::move(cb); }
	void handleMouseClick(double mouseX, double mouseY, int button, int action) override;
	void handleMouseMove(double mouseX, double mouseY) override;

private:
	struct Button {
		float x = 0, y = 0, w = 0, h = 0;
		std::string label;
		bool hovered = false;
		bool enabled = true;
	};

	void onRender() override;
	void build() override;

	void drawTiledBackground();
	void drawTitle();
	void drawSplashText();
	void drawButton(const Button& btn);

	std::function<void(int)> onButtonClick;

	GLuint dirtTexture = 0;

	std::array<Button, 4> buttons;

	std::string splashText;
};

#endif
