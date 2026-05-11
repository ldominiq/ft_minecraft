
#include "ItemPropEntityManager.hpp"
#include "ItemPropEntity.hpp"

ItemPropEntityManager::ItemPropEntityManager(const TextureManager* texMgr)
	: textureManager(texMgr)
{
	shader = std::make_unique<Shader>("shaders/cubePropShader.vert", "shaders/cubePropShader.frag");

	initGL();
}

ItemPropEntityManager::~ItemPropEntityManager()
{
	if (glfwGetCurrentContext()) {
		glDeleteVertexArrays(1, &VAO);
		glDeleteBuffers(1, &VBO);
		glDeleteBuffers(1, &EBO);
	} else {
		VBO = 0;
		VAO = 0;
		EBO = 0;
	}
}

void ItemPropEntityManager::updateMesh(std::vector<std::shared_ptr<ItemEntity>> &entities, const glm::dvec3& eyePos)
{
	int i = -1;
	bool itemsRemoved = false; //Needed because when 1 element is removed the order of the elements change. So when 1 element is removed we redo EVERY prop. Shitty solution but it is what is is.

	static std::vector<float> buffer(MAX_CAPACITY * ITEM_SIZE);
	std::vector<float> vertices;
	vertices.reserve(ITEM_SIZE);

	for (auto entity = entities.begin(); entity != entities.end();)
	{
		if (entity->get()->DoDraw() == false)
		{
			entity++;
			continue ;
		}

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

		//add the meshes of a prop to the back of the buffer
		entity->get()->createMesh(vertices, eyePos, textureManager);
		memcpy(buffer.data() + i * ITEM_SIZE,
			vertices.data(),
			ITEM_SIZE * sizeof(float)
		);

		vertices.clear();
		++entity;
	}

	++i;
	//clear the rest of the buffer. (remove old/non drawable prop entities)
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

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 6 * sizeof(float), (void*)(5 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

void ItemPropEntityManager::draw(const glm::mat4 &projection, const glm::mat4 &view, const glm::dvec3& eyePos, std::vector<std::shared_ptr<ItemEntity>> &entities)
{
	updateMesh(entities, eyePos);

	shader->use();

    glBindVertexArray(VAO);

	// Bind texture array
	if (textureManager) {
		textureManager->bind(GL_TEXTURE0);
		shader->setInt("blockTextures", 0);
	}

	// Vertices are emitted in camera-relative space by createMesh, so use the
	// translation-free view (camera at origin of render space).
	glm::mat4 viewRot = view;
	viewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	shader->setMat4("projection", projection);
	// Note: written as "viewRot" (translation-free) to match the name
	// Lighting::uploadCSMUniforms sets — that call also writes "view" (with
	// translation) which the CSM lookup uses, so they don't collide.
	shader->setMat4("viewRot", viewRot);
	glDrawArrays(GL_TRIANGLES, 0, entities.size() * 36);
}
