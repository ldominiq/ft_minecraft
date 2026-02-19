
#ifndef ITEM_PROP_ENTITY_MANAGER_HPP
#define ITEM_PROP_ENTITY_MANAGER_HPP

#include <glm/glm.hpp>

#include "blockRenderingHelperFunctions.hpp"
#include "Shader.hpp"
#include "ItemEntity.hpp"
#include "GLFW/glfw3.h"

//Not really a manager. More like a drawer....

class ItemPropEntityManager {

	const int ITEM_SIZE = 180;
	const int MAX_CAPACITY = 10000;
	const int MAX_BUFFER_SIZE = ITEM_SIZE * MAX_CAPACITY * sizeof(float); // 1 item takes 180 floats.

	std::unique_ptr<Shader> shader;
	std::vector<float> meshVertices;
	uint texture;
	GLuint VAO, VBO, EBO;

	void updateMesh(std::vector<std::shared_ptr<ItemEntity>> &entities);
	void initGL();

	public:
		ItemPropEntityManager();
		~ItemPropEntityManager();

	void draw(const glm::mat4 &projection, const glm::mat4 &view, std::vector<std::shared_ptr<ItemEntity>> &entities);
};

#endif