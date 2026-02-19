
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

void ItemPropEntityManager::updateMesh(std::vector<std::shared_ptr<ItemEntity>> &entities)
{
	int i = -1;
	bool itemsRemoved = false; //Needed because when 1 element is removed the order of the elements change. So when 1 element is removed we redo EVERY prop. Shitty solution but it is what is is.

	static std::vector<float> buffer(MAX_CAPACITY * ITEM_SIZE);
	std::vector<float> vertices;
	vertices.reserve(ITEM_SIZE);

	for (auto entity = entities.begin(); entity != entities.end();)
	{
		if (entity->get()->removed && !entity->get()->positionUpdated)
		{
			entity = entities.erase(entity);
			itemsRemoved = true;
			continue ;
		}

		++i;

		//attempt to optimize, currently causes rendering issues
		// if (!entity->get()->positionUpdated && !itemsRemoved)
		// {
		// 	entity++;
		// 	continue ;
		// }

		entity->get()->createMesh(vertices);
		memcpy(buffer.data() + i * ITEM_SIZE,
			vertices.data(),
			ITEM_SIZE * sizeof(float)
		);

		vertices.clear();
		++entity;
	}

	++i;
	std::memset(
		buffer.data() + i * ITEM_SIZE,
		0,
		(MAX_CAPACITY - i) * ITEM_SIZE * sizeof(float)
	);

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	void *ptr = glMapBuffer(GL_ARRAY_BUFFER, GL_WRITE_ONLY);

	if (ptr)
	{
		memcpy(ptr, buffer.data(), MAX_CAPACITY * ITEM_SIZE);
		glUnmapBuffer(GL_ARRAY_BUFFER);
	}
}

// In your constructor or init function:
void ItemPropEntityManager::initGL()
{
    glGenVertexArrays(1, &VAO);
    glGenBuffers(1, &VBO);

    glBindVertexArray(VAO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    // Allocate a fixed-size buffer ONCE (say, for up to 1 million floats)
    glBufferData(GL_ARRAY_BUFFER, MAX_BUFFER_SIZE, nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);
}

void ItemPropEntityManager::draw(const glm::mat4 &projection, const glm::mat4 &view, std::vector<std::shared_ptr<ItemEntity>> &entities)
{
	updateMesh(entities);

	shader->use();

    glBindVertexArray(VAO);

	// probably works without because the previous draw already uses the same texture
	// glActiveTexture(GL_TEXTURE0);
	// glBindTexture(GL_TEXTURE_2D, texture);

	shader->setMat4("projection", projection);
	shader->setMat4("view", view);
	glDrawArrays(GL_TRIANGLES, 0, entities.size() * 36);
}
