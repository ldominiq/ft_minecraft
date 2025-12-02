
#include "ItemPropEntityManager.hpp"

ItemPropEntityManager::ItemPropEntityManager()
{
	shader = std::make_unique<Shader>("shaders/cubePropShader.vert", "shaders/cubePropShader.frag");
	texture = shader->loadTexture("assets/textures/textures.png");

	initGL();
}

ItemPropEntityManager::~ItemPropEntityManager()
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

void ItemPropEntityManager::updateMesh(const std::vector<std::shared_ptr<ItemEntity>> &entities)
{
    int i = -1;

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
    for (auto &entity : entities)
	{
		i++;
		if (!entity->positionUpdated) continue ;

		std::vector<float> vertices;
        entity->createMesh(vertices);

		// update the existing data:
    	glBufferSubData(GL_ARRAY_BUFFER, i*180*sizeof(float), 180*sizeof(float), vertices.data());
	}
}

// In your constructor or init function:
void ItemPropEntityManager::initGL()
{
	const int MAX_BUFFER_SIZE = 180 * 10000; // 1 item takes 180 floats.

    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Allocate a fixed-size buffer ONCE (say, for up to 1 million floats)
    glBufferData(GL_ARRAY_BUFFER, MAX_BUFFER_SIZE * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void ItemPropEntityManager::draw(const glm::mat4 &projection, const glm::mat4 &view, const std::vector<std::shared_ptr<ItemEntity>> &entities)
{
	updateMesh(entities);

	shader->use();

    glBindVertexArray(VAO);

	// glActiveTexture(GL_TEXTURE0);
	// glBindTexture(GL_TEXTURE_2D, texture);

	shader->setMat4("projection", projection);
	shader->setMat4("view", view);
	glDrawArrays(GL_TRIANGLES, 0, entities.size() * 36); // TODO : check if we can pass les than meshVertices.size()
}
