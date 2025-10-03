
#ifndef ITEM_PROP_ENTITY
#define ITEM_RPOP__ENTITY

#include <glm/glm.hpp>

#include "ItemEntity.hpp"
#include "Shader.hpp"
#include "blockRenderingHelperFunctions.hpp"
#include "GLFW/glfw3.h"

class ItemPropEntity : public ItemEntity
{
	GLuint VAO, VBO, EBO;
	uint texture;
	std::unique_ptr<Shader> shader;
	std::vector<float> meshVertices;

	void uploadMesh();

	public:
		ItemPropEntity(glm::vec3 position, BlockType ID, entityID entityID);
		~ItemPropEntity();

		void draw(const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &position);
};

#endif