
#ifndef CHUNK_RENDERER_HPP
#define CHUNK_RENDERER_HPP

#include <glad/glad.h>
#include <cstring>
#include <vector>

#include "GLFW/glfw3.h"
#include <glm/glm.hpp>
#include "Chunk.hpp"
#include "TextureManager.hpp"
#include "blockRenderingHelperFunctions.hpp"
#include "VegetationRenderer.hpp"
#include "PackedVertex.hpp"
#include <memory>

#ifdef _WIN32
typedef unsigned int uint;
#endif

class ChunkRenderer : public Chunk {

    GLuint VAO = 0;
    GLuint VBO = 0;
	uint meshVertexCount = 0;                      // number of PackedVertex entries (NOT bytes / floats)
    std::vector<PackedVertex> meshVertices;        // 8 bytes per vertex (was 11 floats)

	GLuint waterVAO = 0;
	GLuint waterVBO = 0;
	uint waterMeshVertexCount = 0;
	std::vector<PackedVertex> waterMeshVertices;

	const TextureManager* textureManager = nullptr;

	std::unique_ptr<VegetationRenderer> vegetationRenderer;

    void addFace(int x, int y, int z, BlockType type, int face, float skyLightLevel); // Add a face to the mesh vertices (solid blocks)
	void addWaterFace(int x, int y, int z, int face, float skyLightLevel); // Add a face to water mesh

	public:

		ChunkRenderer(std::istream& in);
		~ChunkRenderer();

		bool needsUpdate = false;
		bool neighbourNeedUpdate[4] { false };

		glm::vec3 cachedMinP;
		glm::vec3 cachedMaxP;
		const glm::vec3& getCachedMinP() const { return cachedMinP; }
		const glm::vec3& getCachedMaxP() const { return cachedMaxP; }

		// Set the TextureManager (must be called before building meshes)
		void setTextureManager(const TextureManager* tm) { textureManager = tm; }

		// Release GL resources
		void releaseGL();

		void updateMesh();
		void buildMesh(); // Build both solid and water meshes
		void buildMeshData();
		void uploadMesh();
		void buildVegetationMesh() const; // Build vegetation instanced mesh

		inline const GLuint getVao() const {return VAO;}
		inline const uint getMeshVertexCount() const {return meshVertexCount;}

		inline const GLuint getWaterVao() const {return waterVAO;}
		inline const uint getWaterMeshVertexCount() const {return waterMeshVertexCount;}

		inline VegetationRenderer* getVegetationRenderer() const { return vegetationRenderer.get(); }
};

#endif
