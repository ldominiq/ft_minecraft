#ifndef SETTINGSMENU_HPP
#define SETTINGSMENU_HPP

#include "Menu.hpp"
#include <functional>

class SettingsMenu : public Menu {
public:
	SettingsMenu(float width, float height, GLuint dirtTex);

	void setDoneCallback(std::function<void()> cb) { onDone = std::move(cb); }
	void setChangeControlsCallback(std::function<void()> cb) { changeControls = std::move(cb); }

	void handleMouseClick(double mouseX, double mouseY, int button, int action) override;
	void handleMouseMove(double mouseX, double mouseY) override;

	void addChar(char c);
	void removeChar();
	std::string getUsername() const { return username; }

private:
	void onRender() override;
	void build() override;

	std::string username;

	std::function<void()> onDone;
	std::function<void()> changeControls;

	GLuint dirtTexture = 0;
	Button doneButton;
	Button changeControlsButton;
	Button nameBox; // box
};

#endif
