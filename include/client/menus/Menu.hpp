
#ifndef MENU_HPP
#define MENU_HPP

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "Shader.hpp"
#include "Typer.hpp"
#include "stb_image.h"

class Menu {

	protected:
		struct Button {
			float x = 0, y = 0, w = 0, h = 0;
			std::string label;
			bool hovered = false;
			bool enabled = true;
		};

		struct Toggle {
			Button btn;
			std::string label;
			std::function<bool()> get;
			std::function<void(bool)> set;
		};

		struct Slider {
			Button track;
			std::string label;
			bool isInt = false;
			int precision = 0; // decimal places for float sliders
			float minVal = 0.0f;
			float maxVal = 1.0f;
			std::function<float()> getF;
			std::function<void(float)> setF;
			std::function<int()> getI;
			std::function<void(int)> setI;
			bool dragging = false;
		};
		
		int DESIGN_WIDTH = 0;
		int DESIGN_HEIGHT = 0;

		int fullscreenWidth = 0;
		int fullscreenHeight = 0;

		int menuWidth = 0;
		int menuHeight = 0;
		float menuScale = 0;
		float textScale = 1.0f;

		Typer textRenderer;
		Typer titleRenderer;
		GLuint VAO = 0;
		GLuint VBO = 0;
		GLuint textureVAO = 0;
		GLuint textureVBO = 0;
		std::unique_ptr<Shader> simpleQuadShader;
		std::unique_ptr<Shader> texturedQuadShader;

		void drawSimpleQuad(float x, float y, float w, float h, const glm::vec4 &color) const;
		void drawTexturedQuad(float x, float y, float w, float h, unsigned int textureID, float alpha = 1.0f);
		void drawTiledTexturedQuad(float x, float y, float w, float h, unsigned int textureID, float tileSize);
		void drawTiledBackground(GLuint &texture);
		void drawButton(float x, float y, float w, float h, const std::string& label, bool hovered, bool enabled = true);
		void drawInputBox(float x, float y, float w, float h, const std::string& text, bool focused);
		void drawToggle(const Toggle& t);
		void drawSlider(const Slider& s);

		static bool isInside(const Button& b, float glX, float glY);
		static void updateHover(Button& b, float glX, float glY);

		bool clickToggle(Toggle& t, float glX, float glY);
		bool clickSlider(Slider& s, float glX, float glY);
		void dragSlider(Slider& s, float glX);

	private:
		static void applySliderAtX(Slider& s, float glX);

	public:
		static GLuint loadTexture2D(const char* path, bool pixelated = true, int* outWidth = nullptr, int* outHeight = nullptr);

	protected:

		virtual void onRender() = 0;
		// Called on construction and on every window resize - recalculate pixel positions here
		virtual void build() {};

	public:
		virtual ~Menu();
		Menu(float width, float height);

		virtual void handleMouseClick(double mouseX, double mouseY, int button, int action) {};
		virtual void handleMouseMove(double mouseX, double mouseY) {};
		virtual void resize(float width, float height);
		void render();
};

#endif
