
#ifndef ITEM_PROP_ENTITY
#define ITEM_RPOP__ENTITY

#include "ItemEntity.hpp"
#include "Shader.hpp"

class ItemPropEntity : public ItemEntity
{
	GLuint VAO, VBO, EBO;
	uint texture;
	std::unique_ptr<Shader> shader;

	void uploadMesh();

	public:
		ItemPropEntity(glm::vec3 position, float yaw, ItemID item);
		~ItemPropEntity();

		void draw(const glm::mat4 &projection, const glm::mat4 &view);
};

#endif