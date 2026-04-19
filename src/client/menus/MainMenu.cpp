#include "MainMenu.hpp"
#include <cstdlib>
#include <ctime>
#include <cmath>
#include <GLFW/glfw3.h>

static const char* splashTexts[] = {
	"Also try Terraria!",
	"42 school project!",
	"Now with water!",
	"100% organic blocks",
	"Dirt is beautiful!",
	"Open source!",
	"Not affiliated with Mojang!",
	"Powered by OpenGL!",
	"Chunky!",
	"Procedurally generated!",
	"UDP all the things!",
	"As seen on GitHub!",
	"Now with shadows!",
	"SSAO included!",
	"Biome diversity!",
};
static constexpr int splashCount = sizeof(splashTexts) / sizeof(splashTexts[0]);

static constexpr float DESIGN_W = 960.0f;
static constexpr float DESIGN_H = 540.0f;

// Button layout in design coordinates
static constexpr float BTN_W = 300.0f;
static constexpr float BTN_H = 30.0f;
static constexpr float BTN_SPACING = 8.0f;

MainMenu::MainMenu(float width, float height)
	: Menu(DESIGN_W, DESIGN_H)
{
	dirtTexture = loadTexture2D("assets/textures/block/dirt.png");

	int titleW = 0, titleH = 0;
	titleTexture = loadTexture2D("assets/textures/FT-MINECRAFT.png", false, &titleW, &titleH);
	if (titleTexture && titleH > 0)
		titleTexAspect = static_cast<float>(titleW) / static_cast<float>(titleH);

	srand(static_cast<unsigned>(time(nullptr)));
	splashText = splashTexts[rand() % splashCount];

	buttons[0].label = "Singleplayer";
	buttons[0].enabled = false;
	buttons[1].label = "Multiplayer";
	buttons[2].label = "Settings";
	buttons[3].label = "Quit";

	resize(width, height);
}

MainMenu::~MainMenu()
{
	if (glfwGetCurrentContext()) {
		if (dirtTexture)  glDeleteTextures(1, &dirtTexture);
		if (titleTexture) glDeleteTextures(1, &titleTexture);
	}
}

void MainMenu::build()
{
	float centerX = fullscreenWidth / 2.0f;
	float centerY = fullscreenHeight / 2.0f;

	float btnW = BTN_W * menuScale;
	float btnH = BTN_H * menuScale;
	float spacing = BTN_SPACING * menuScale;

	// Stack 4 buttons vertically, positioned in the lower portion of the screen
	float totalHeight = 4 * btnH + 3 * spacing;
	float startY = centerY - totalHeight / 2.0f - 50.0f * menuScale;

	for (int i = 0; i < 4; i++) {
		buttons[i].w = btnW;
		buttons[i].h = btnH;
		buttons[i].x = centerX - btnW / 2.0f;
		buttons[i].y = startY + (3 - i) * (btnH + spacing); // bottom-up in OpenGL coords
	}
}

void MainMenu::drawTiledBackground()
{
	float tileSize = 64.0f * menuScale;
	drawTiledTexturedQuad(0, 0, fullscreenWidth, fullscreenHeight, dirtTexture, tileSize);

	// Darken overlay
	drawSimpleQuad(0, 0, fullscreenWidth, fullscreenHeight, glm::vec4(0.0f, 0.0f, 0.0f, 0.4f));
}

void MainMenu::drawTitle()
{
	if (titleTexture) {
		float titleH = 140.0f * menuScale;
		float titleW = titleH * titleTexAspect;
		float titleX = (fullscreenWidth - titleW) / 2.0f;
		float titleY = fullscreenHeight - (titleH + 30.0f * menuScale);
		drawTexturedQuad(titleX, titleY, titleW, titleH, titleTexture, 1.0f);
		return;
	}

	// Fallback: render as text if PNG is missing
	float savedScale = textRenderer.getScale();
	float titleScale = 1.8f * menuScale;
	textRenderer.setScale(titleScale);
	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	std::string title = "FT_MINECRAFT";
	float titleWidth = textRenderer.getPixelSizeOfString(title);
	float titleX = (fullscreenWidth - titleWidth) / 2.0f;
	float titleY = fullscreenHeight - 120.0f * menuScale;

	textRenderer.renderText(title, titleX, titleY, glm::vec3(1.0f, 1.0f, 1.0f));
	textRenderer.setScale(savedScale);
}

void MainMenu::drawSplashText()
{
	float savedScale = textRenderer.getScale();

	float pulse = 1.0f + 0.08f * std::sin(glfwGetTime() * 3.0f);
	float splashScale = 0.35f * menuScale * pulse;
	textRenderer.setScale(splashScale);
	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	// Anchor near the title's lower-right corner and rotated
	float titleH = 140.0f * menuScale;
	float titleW = titleH * titleTexAspect;
	float titleCX = fullscreenWidth / 2.0f;
	float titleY = fullscreenHeight - (titleH + 30.0f * menuScale);
	float anchorX = titleCX + titleW * 0.42f;
	float anchorY = titleY + titleH * 0.3f;

	// Offset the text so it's centered on the anchor (pre-rotation)
	float splashWidth = textRenderer.getPixelSizeOfString(splashText);
	const float rotDeg = 20.0f;
	const float rad = rotDeg * 3.14159265358979323846f / 180.0f;
	float startX = anchorX - (splashWidth / 2.0f) * std::cos(rad);
	float startY = anchorY - (splashWidth / 2.0f) * std::sin(rad);

	textRenderer.renderText(splashText, startX, startY, glm::vec3(1.0f, 1.0f, 0.0f), 1.0f, rotDeg);

	textRenderer.setScale(savedScale);
}

void MainMenu::drawButton(const Button& btn)
{
	glm::vec4 bgColor;
	if (!btn.enabled)
		bgColor = glm::vec4(0.2f, 0.2f, 0.2f, 0.8f);
	else if (btn.hovered)
		bgColor = glm::vec4(0.4f, 0.4f, 0.5f, 0.9f);
	else
		bgColor = glm::vec4(0.25f, 0.25f, 0.3f, 0.85f);

	// Button border
	float border = 2.0f * menuScale;
	glm::vec4 borderColor = btn.hovered && btn.enabled
		? glm::vec4(0.7f, 0.7f, 0.8f, 1.0f)
		: glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
	drawSimpleQuad(btn.x - border, btn.y - border, btn.w + 2 * border, btn.h + 2 * border, borderColor);

	// Button background
	drawSimpleQuad(btn.x, btn.y, btn.w, btn.h, bgColor);

	// Button label
	float savedScale = textRenderer.getScale();
	float labelScale = 0.5f * menuScale;
	textRenderer.setScale(labelScale);
	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	float labelWidth = textRenderer.getPixelSizeOfString(btn.label);
	float ascent = textRenderer.getAscent();
	float labelX = btn.x + (btn.w - labelWidth) / 2.0f;
	float labelY = btn.y + (btn.h - ascent) / 2.0f;

	glm::vec3 textColor = btn.enabled ? glm::vec3(1.0f) : glm::vec3(0.5f);
	textRenderer.renderText(btn.label, labelX, labelY, textColor);

	textRenderer.setScale(savedScale);
}

void MainMenu::onRender()
{
	drawTiledBackground();
	drawTitle();

	for (auto& btn : buttons)
		drawButton(btn);

	drawSplashText();

	// Version text (bottom-left)
	float savedScale = textRenderer.getScale();
	float smallScale = 0.35f * menuScale;
	textRenderer.setScale(smallScale);
	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	textRenderer.renderText("ft_minecraft v0.1", 10.0f * menuScale, 10.0f * menuScale, glm::vec3(0.7f));

	// Credits (bottom-right)
	std::string credits = "ldominiq, lskraber";
	float creditsWidth = textRenderer.getPixelSizeOfString(credits);
	textRenderer.renderText(credits, fullscreenWidth - creditsWidth - 10.0f * menuScale, 10.0f * menuScale, glm::vec3(0.7f));

	textRenderer.setScale(savedScale);
}

void MainMenu::handleMouseMove(double mouseX, double mouseY)
{
	// Convert from window coords (top-left origin) to OpenGL coords (bottom-left origin)
	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	for (auto& btn : buttons) {
		btn.hovered = btn.enabled &&
			glX >= btn.x && glX <= btn.x + btn.w &&
			glY >= btn.y && glY <= btn.y + btn.h;
	}
}

void MainMenu::handleMouseClick(double mouseX, double mouseY, int button, int action)
{
	if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
		return;

	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	for (int i = 0; i < 4; i++) {
		if (!buttons[i].enabled) continue;
		if (glX >= buttons[i].x && glX <= buttons[i].x + buttons[i].w &&
			glY >= buttons[i].y && glY <= buttons[i].y + buttons[i].h) {
			if (onButtonClick) onButtonClick(i);
			return;
		}
	}
}
