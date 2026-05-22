#ifndef SOUNDSMENU_HPP
#define SOUNDSMENU_HPP

#include "Menu.hpp"
#include <functional>
#include <vector>

class SoundsMenu : public Menu {
public:
	SoundsMenu(float width, float height, GLuint dirtTex);

	void setDoneCallback(std::function<void()> cb) { onDone = std::move(cb); }

	void handleMouseClick(double mouseX, double mouseY, int button, int action) override;
	void handleMouseMove(double mouseX, double mouseY) override;

	// Lay out widgets after all add* calls. The caller adds widgets up front, then
	// calls this once
	void commit() { build(); }

private:
	void onRender() override;
	void build() override;

	Button doneButton;
	GLuint dirtTexture = 0;

	std::function<void()> onDone;
};

#endif
