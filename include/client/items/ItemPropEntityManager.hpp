
#ifndef ITEM_PROP_ENTITY_MANAGER_HPP
#define ITEM_PROP_ENTITY_MANAGER_HPP

#include <glm/glm.hpp>

#include "blockRenderingHelperFunctions.hpp"
#include "Shader.hpp"
#include "TextureManager.hpp"
#include "ItemEntity.hpp"
#include "GLFW/glfw3.h"

//Not really a manager. More like a drawer....

class ItemPropEntityManager {

	const int ITEM_SIZE = 216; // 6 faces * 6 verts * 6 floats = 216 floats now
	const int MAX_CAPACITY = 10000;
	const int MAX_BUFFER_SIZE = ITEM_SIZE * MAX_CAPACITY * sizeof(float);

	std::unique_ptr<Shader> shader;
	std::vector<float> meshVertices;
	const TextureManager* textureManager = nullptr;
	GLuint VAO, VBO, EBO;

	// Repack the per-entity vertex slots into the GPU buffer, skipping entities
	// whose DoDraw() returns false. Returns the number of slots actually
	// written — draw() uses this so glDrawArrays issues vertices only for
	// rendered items instead of dragging the whole vector through the GPU.
	size_t updateMesh(std::vector<std::shared_ptr<ItemEntity>> &entities, const glm::dvec3& eyePos);
	void initGL();

	public:
		ItemPropEntityManager(const TextureManager* texMgr);
		~ItemPropEntityManager();

	void draw(const glm::mat4 &projection, const glm::mat4 &view, const glm::dvec3& eyePos,
	          std::vector<std::shared_ptr<ItemEntity>> &entities);

	// Exposed so App.cpp can upload dirLight/pointLights/CSM uniforms before draw().
	// entity_lighting.glsl reuses the same uniform names lighting.frag does.
	Shader& getShader() { return *shader; }
};

#endif