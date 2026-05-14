
#ifndef ITEM_PROP_ENTITY_MANAGER_HPP
#define ITEM_PROP_ENTITY_MANAGER_HPP

#include <glm/glm.hpp>

#include "blockRenderingHelperFunctions.hpp"
#include "Shader.hpp"
#include "TextureManager.hpp"
#include "ItemEntity.hpp"
#include "GLFW/glfw3.h"

class ICommonWorld;

//Not really a manager. More like a drawer....

class ItemPropEntityManager {

	// 6 faces * 6 verts * 7 floats (pos3 + uv2 + texLayer + skyLight) per cube.
	// Sprite/billboard items pad up to the same 36-vert/252-float slot.
	const int ITEM_SIZE = 252;
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
	// `world` is used to sample chunk sky-light at each item's position so
	// items darken in caves like terrain does; may be null (defaults to 1.0).
	size_t updateMesh(std::vector<std::shared_ptr<ItemEntity>> &entities, const glm::dvec3& eyePos, const ICommonWorld* world);
	void initGL();

	public:
		ItemPropEntityManager(const TextureManager* texMgr);
		~ItemPropEntityManager();

	void draw(const glm::mat4 &projection, const glm::mat4 &view, const glm::dvec3& eyePos,
	          std::vector<std::shared_ptr<ItemEntity>> &entities, const ICommonWorld* world);

	// Exposed so App.cpp can upload dirLight/pointLights/CSM uniforms before draw().
	// entity_lighting.glsl reuses the same uniform names lighting.frag does.
	Shader& getShader() { return *shader; }
};

#endif