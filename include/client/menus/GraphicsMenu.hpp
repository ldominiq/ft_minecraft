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

	// Lay out widgets after all add* calls. The caller adds widgets up front, then
	// calls this once
	void commit() { build(); }

private:
	void onRender() override;
	void build() override;

	std::vector<Toggle> toggles;

	Button doneButton;
	GLuint dirtTexture = 0;

	std::function<void()> onDone;
};

#endif
