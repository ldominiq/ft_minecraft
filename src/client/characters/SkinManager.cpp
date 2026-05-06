#include "SkinManager.hpp"

#include "GLFW/glfw3.h"
#include "stb_image.h"
#include <iostream>

SkinManager::~SkinManager()
{
	if (!glfwGetCurrentContext())
		return;
	for (auto& [_, tex] : skins)
	{
		if (tex)
			glDeleteTextures(1, &tex);
	}
}

bool SkinManager::load(const std::string& name, const std::string& path)
{
	int w = 0, h = 0, channels = 0;
	stbi_set_flip_vertically_on_load(false);
	unsigned char* data = stbi_load(path.c_str(), &w, &h, &channels, 4);
	if (!data)
	{
		std::cerr << "SkinManager: failed to load skin '" << name << "' from " << path << std::endl;
		return false;
	}

	GLuint tex = 0;
	glGenTextures(1, &tex);
	glBindTexture(GL_TEXTURE_2D, tex);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glBindTexture(GL_TEXTURE_2D, 0);

	stbi_image_free(data);

	auto it = skins.find(name);
	if (it != skins.end() && it->second)
		glDeleteTextures(1, &it->second);
	skins[name] = tex;

	std::cout << "Loaded skin '" << name << "' (" << w << "x" << h << ") from " << path << std::endl;
	return true;
}

GLuint SkinManager::get(const std::string& name) const
{
	auto it = skins.find(name);
	return (it != skins.end()) ? it->second : 0;
}
