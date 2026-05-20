#ifndef GRAPHICSMENU_HPP
#define GRAPHICSMENU_HPP

#include "Menu.hpp"
#include <functional>
#include <vector>

class GraphicsMenu : public Menu {
public:
	GraphicsMenu(float width, float height, GLuint dirtTex);

	void setDoneCallback(std::function<void()> cb) { onDone = std::move(cb); }

	void handleMouseClick(double mouseX, double mouseY, int button, int action) override;
	void handleMouseMove(double mouseX, double mouseY) override;

	void addToggle(const std::string& label,
				   std::function<bool()> get,
				   std::function<void(bool)> set);

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

	std::vector<Toggle> toggles;
	std::vector<Slider> sliders;

	Button doneButton;
	GLuint dirtTexture = 0;

	std::function<void()> onDone;
};

#endif
