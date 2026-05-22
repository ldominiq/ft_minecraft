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

	void addFloatSlider(const std::string& label, float min, float max,
						std::function<float()> get,
						std::function<void(float)> set,
						int precision = 0);

	void addIntSlider(const std::string& label, int min, int max,
					  std::function<int()> get,
					  std::function<void(int)> set);

	// Lay out widgets after all add* calls. The caller adds widgets up front, then
	// calls this once
	void commit() { build(); }

private:
	void onRender() override;
	void build() override;

	std::vector<Slider> sliders;

	Button doneButton;
	GLuint dirtTexture = 0;

	std::function<void()> onDone;
};

#endif
