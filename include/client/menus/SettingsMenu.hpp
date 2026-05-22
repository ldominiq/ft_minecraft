#ifndef SETTINGSMENU_HPP
#define SETTINGSMENU_HPP

#include "Menu.hpp"
#include <functional>

class SettingsMenu : public Menu {
public:
	SettingsMenu(float width, float height, GLuint dirtTex);

	void setDoneCallback(std::function<void()> cb) {
		onDone = [this, cb = std::move(cb)]()
		{
			saveUsername();

			if (cb)
				cb();
		};
	}
	void setChangeControlsCallback(std::function<void()> cb) { changeControls = std::move(cb); }
	void setGraphicsCallback(std::function<void()> cb) { onGraphics = std::move(cb); }
	void setSoundsCallback(std::function<void()> cb) { onSounds = std::move(cb); }

	void handleMouseClick(double mouseX, double mouseY, int button, int action) override;
	void handleMouseMove(double mouseX, double mouseY) override;

	void addChar(char c);
	void removeChar();
	std::string getUsername() const { return username; }
	void setUsername(const std::string& name) { username = name; saveUsername(); }

	void setUsernameEditable(bool editable) { usernameEditable = editable; }

private:
	void onRender() override;
	void build() override;

	void saveUsername(const char* filename = "username.cfg");
	void loadUsername(const char* filename = "username.cfg");

	std::string username;
	bool usernameEditable = true;

	std::function<void()> onDone;
	std::function<void()> changeControls;
	std::function<void()> onGraphics;
	std::function<void()> onSounds;

	GLuint dirtTexture = 0;
	Button doneButton;
	Button changeControlsButton;
	Button graphicsButton;
	Button soundsButton;
	Button nameBox; // box
};

#endif
