
#include "ItemPropEntity.hpp"

ItemPropEntity::ItemPropEntity(glm::vec3 position, float yaw, ItemID item) : ItemEntity(position, yaw, item)
{
	texture = loadTexture("assets/textures/textures.png");
	shader = std::make_unique<Shader>("itemProp.vert", "itemProp.frag");

	uploadMesh();
}

ItemPropEntity::~ItemPropEntity()
{
	glDeleteTextures(1, &texture);
	glDeleteVertexArrays(1, &VAO);
	glDeleteBuffers(1, &VBO);
	glDeleteBuffers(1, &EBO);
}

void ItemPropEntity::uploadMesh()
{
	float quadVertices[] = {
		// positions      // texcoords
	-0.5f, -0.5f, 0.0f,  0.0f, 0.0f,  // bottom left
		0.5f, -0.5f, 0.0f,  1.0f, 0.0f,  // bottom right
		0.5f,  0.5f, 0.0f,  1.0f, 1.0f,  // top right
	-0.5f,  0.5f, 0.0f,  0.0f, 1.0f   // top left
	};

	unsigned int quadIndices[] = {
		0, 1, 2,
		2, 3, 0
	};

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glGenBuffers(1, &EBO);

	glBindVertexArray(VAO);

	// vertex buffer
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

	// index buffer
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
	glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(quadIndices), quadIndices, GL_STATIC_DRAW);

	// position attribute (layout = 0)
	glVertexAttribPointer(
		0,                  // location = 0 in shader
		3,                  // vec3
		GL_FLOAT, 
		GL_FALSE, 
		5 * sizeof(float),  // stride: 3 pos + 2 tex = 5 floats per vertex
		(void*)0            // offset: starts at beginning
	);
	glEnableVertexAttribArray(0);

	// texcoord attribute (layout = 1)
	glVertexAttribPointer(
		1, 
		2, 
		GL_FLOAT, 
		GL_FALSE, 
		5 * sizeof(float), 
		(void*)(3 * sizeof(float)) // offset: after 3 floats of position
	);
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
}

void ItemPropEntity::draw(const glm::mat4 &projection, const glm::mat4 &view)
{
	shader->use();

	shader->setMat4("projection", projection);
	shader->setMat4("view", view);
	shader->setVec3("propPosition",position);

	shader->setInt("blockTexture", 0);

	glBindVertexArray(VAO);
	glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
}