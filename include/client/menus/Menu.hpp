
#ifndef MENU_HPP
#define MENU_HPP

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <GLFW/glfw3.h>

#include <memory>
#include <string>
#include <vector>

#include "Shader.hpp"
#include "Typer.hpp"
#include "stb_image.h"

class Menu {

	protected:
		int DESIGN_WIDTH = 0;
		int DESIGN_HEIGHT = 0;

		int fullscreenWidth = 0;
		int fullscreenHeight = 0;

		int menuWidth = 0;
		int menuHeight = 0;
		float menuScale = 0;
		float textScale = 0.3f;

		Typer textRenderer;
		GLuint VAO = 0;
		GLuint VBO = 0;
		GLuint textureVAO = 0;
		GLuint textureVBO = 0;
		std::unique_ptr<Shader> simpleQuadShader;
		std::unique_ptr<Shader> texturedQuadShader;

		void drawSimpleQuad(float x, float y, float w, float h, const glm::vec4 &color) const;
		void drawTexturedQuad(float x, float y, float w, float h, unsigned int textureID, float alpha = 1.0f); //untested. Vibe coded.
		void drawTiledTexturedQuad(float x, float y, float w, float h, unsigned int textureID, float tileSize);

	public:
		static GLuint loadTexture2D(const char* path);

	protected:

		virtual void onRender() = 0;
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
