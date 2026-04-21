#ifndef SETTINGSMENU_HPP
#define SETTINGSMENU_HPP

#include "Menu.hpp"
#include <functional>

class SettingsMenu : public Menu {
public:
	SettingsMenu(float width, float height, GLuint dirtTex);

	void setDoneCallback(std::function<void()> cb) { onDone = std::move(cb); }

	void handleMouseClick(double mouseX, double mouseY, int button, int action) override;
	void handleMouseMove(double mouseX, double mouseY) override;

private:
	void onRender() override;
	void build() override;

	std::function<void()> onDone;

	GLuint dirtTexture = 0;
	Button doneButton;
};

#endif
