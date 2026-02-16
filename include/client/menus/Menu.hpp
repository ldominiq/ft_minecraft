
#ifndef MENU_HPP
#define MENU_HPP

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <memory>
#include <string>
#include <vector>

#include "Shader.hpp"

class Menu {

	protected:
		// TODO : add a hook to update it if we ever implement an option to modify resolution
		int width = 0;
		int height = 0;

	protected:
		GLuint VAO = 0;
		GLuint VBO = 0;
		GLuint fontTexture;
		std::unique_ptr<Shader> simpleQuadShader;
		GLuint tex = 0;

		void drawQuad();
		void drawSimpleQuad(float x, float y, float w, float h, const glm::vec4 &color) const; //no texture quad
		void drawText(float x, float y, const std::string& text);

		virtual void onRender() = 0;

	public:
		virtual ~Menu() = default;
		Menu(float width, float height);

		void render();
};

#endif
