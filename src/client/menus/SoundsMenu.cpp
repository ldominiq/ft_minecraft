#include "SoundsMenu.hpp"
#include <GLFW/glfw3.h>

static constexpr float DESIGN_W = 960.0f;
static constexpr float DESIGN_H = 540.0f;

static constexpr float ROW_W = 280.0f;
static constexpr float ROW_H = 30.0f;
static constexpr float ROW_GAP = 10.0f;

static constexpr float DONE_W = 200.0f;
static constexpr float DONE_H = 40.0f;

SoundsMenu::SoundsMenu(float width, float height, GLuint dirtTex)
	: Menu(DESIGN_W, DESIGN_H), dirtTexture(dirtTex)
{
	doneButton.label = "Done";
	resize(width, height);
}

void SoundsMenu::addFloatSlider(const std::string& label, float min, float max,
								  std::function<float()> get,
								  std::function<void(float)> set,
								  int precision)
{
	Slider s;
	s.label = label;
	s.minVal = min;
	s.maxVal = max;
	s.isInt = false;
	s.precision = precision;
	s.getF = std::move(get);
	s.setF = std::move(set);
	sliders.push_back(std::move(s));
}

void SoundsMenu::addIntSlider(const std::string& label, int min, int max,
								std::function<int()> get,
								std::function<void(int)> set)
{
	Slider s;
	s.label = label;
	s.minVal = static_cast<float>(min);
	s.maxVal = static_cast<float>(max);
	s.isInt = true;
	s.getI = std::move(get);
	s.setI = std::move(set);
	sliders.push_back(std::move(s));
}

void SoundsMenu::build()
{
	float centerX = fullscreenWidth / 2.0f;
	float rowW = ROW_W * menuScale;
	float rowH = ROW_H * menuScale;
	float rowGap = ROW_GAP * menuScale;

	float topY = fullscreenHeight - 130.0f * menuScale;
	float y = topY - rowH;

	for (auto& s : sliders)
	{
		s.track.x = centerX - rowW / 2.0f;
		s.track.y = y;
		s.track.w = rowW;
		s.track.h = rowH;
		y -= rowH + rowGap;
	}

	doneButton.w = DONE_W * menuScale;
	doneButton.h = DONE_H * menuScale;
	doneButton.x = centerX - doneButton.w / 2.0f;
	doneButton.y = 60.0f * menuScale;
}

void SoundsMenu::handleMouseClick(double mouseX, double mouseY, int button, int action)
{
	if (button != GLFW_MOUSE_BUTTON_LEFT) return;

	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	if (action == GLFW_RELEASE) {
		for (auto& s : sliders) s.dragging = false;
		return;
	}

	if (action != GLFW_PRESS) return;

	for (auto& s : sliders)
		if (clickSlider(s, glX, glY)) return;

	if (isInside(doneButton, glX, glY))
	{
		if (onDone) onDone();
	}
}

void SoundsMenu::handleMouseMove(double mouseX, double mouseY)
{
	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	for (auto& s : sliders)
	{
		updateHover(s.track, glX, glY);
		dragSlider(s, glX);
	}

	updateHover(doneButton, glX, glY);
}

void SoundsMenu::onRender()
{
	drawTiledBackground(dirtTexture);

	float savedScale = textRenderer.getScale();

	titleRenderer.setScale(0.4f * menuScale);
	titleRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	std::string title = "Sounds";
	float titleWidth = titleRenderer.getPixelSizeOfString(title);
	float titleX = (fullscreenWidth - titleWidth) / 2.0f;
	float titleY = fullscreenHeight - 80.0f * menuScale;
	titleRenderer.renderText(title, titleX, titleY, glm::vec3(1.0f));

	for (auto& s : sliders)
		drawSlider(s);

	drawButton(doneButton.x, doneButton.y, doneButton.w, doneButton.h,
			   doneButton.label, doneButton.hovered);

	textRenderer.setScale(savedScale);
}
