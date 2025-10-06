
#ifndef ITEM_PROP_ENTITY_MANAGER_HPP
#define ITEM_PROP_ENTITY_MANAGER_HPP

#include <glm/glm.hpp>

#include "blockRenderingHelperFunctions.hpp"
#include "Shader.hpp"
#include "ItemEntity.hpp"
#include "GLFW/glfw3.h"

class ItemPropEntityManager {

	std::unique_ptr<Shader> shader;
	std::vector<float> meshVertices;
	uint texture;
	GLuint VAO, VBO, EBO;

	void updateMesh(const std::vector<std::shared_ptr<Entity>> &entities);
	void initGL();

	public:
		ItemPropEntityManager();
		~ItemPropEntityManager();

	void draw(const glm::mat4 &projection, const glm::mat4 &view, const std::vector<std::shared_ptr<Entity>> &entities);
};

#endif