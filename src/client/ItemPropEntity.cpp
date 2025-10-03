
#include "ItemPropEntity.hpp"

ItemPropEntity::ItemPropEntity(glm::vec3 position, BlockType ID, entityID entityID) : ItemEntity(position, ID, entityID)
{
	texture = loadTexture("assets/textures/textures.png");
	shader = std::make_unique<Shader>("shaders/cubePropShader.vert", "shaders/cubePropShader.frag");

	uploadMesh();
}

ItemPropEntity::~ItemPropEntity()
{
	if (glfwGetCurrentContext()) {
		glDeleteTextures(1, &texture);
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
	} else {
		VBO = 0;
		VAO = 0;
		EBO = 0;
	}

}

void ItemPropEntity::uploadMesh()
{
	buildCube(meshVertices, position.x, position.y, position.z, 0, 0, item);
    // Light cube setup
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);
    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    
    glBufferData(GL_ARRAY_BUFFER, meshVertices.size() * sizeof(float), meshVertices.data(), GL_DYNAMIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
	glEnableVertexAttribArray(0);

	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void ItemPropEntity::draw(const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &position)
{
	shader->use();
    glBindVertexArray(VAO);

	auto model = glm::mat4(1.0f);
	model = glm::translate(model, position);
	model = glm::translate(model, glm::vec3(0.0f, entityHeight / 2.0f, 0.0f));
	model = glm::scale(model, glm::vec3(0.2f)); // Make it a smaller cube

	shader->setInt("atlas", 0);
	shader->setMat4("model", model);
	shader->setMat4("projection", projection);
	shader->setMat4("view", view);
	glDrawArrays(GL_TRIANGLES, 0, 36);
}
