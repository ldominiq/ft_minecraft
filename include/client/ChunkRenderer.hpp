
#ifndef CHUNK_RENDERER_HPP
#define CHUNK_RENDERER_HPP

#include <glad/glad.h>
#include <cstring>

#include "GLFW/glfw3.h"
#include "Chunk.hpp"
#include "TextureManager.hpp"
#include "blockRenderingHelperFunctions.hpp"

class ChunkRenderer : public Chunk {

    GLuint VAO = 0;
    GLuint VBO = 0;
	uint meshVerticesSize = 0;
    std::vector<float> meshVertices; // Vertices for the mesh

	GLuint waterVAO = 0;
	GLuint waterVBO = 0;
	uint waterMeshVerticesSize = 0;
	std::vector<float> waterMeshVertices;

	const TextureManager* textureManager = nullptr;

    void addFace(int x, int y, int z, BlockType type, int face, float skyLightLevel); // Add a face to the mesh vertices (solid blocks)
	void addWaterFace(int x, int y, int z, int face, float skyLightLevel); // Add a face to water mesh

	public:

		ChunkRenderer(std::istream& in);
		~ChunkRenderer();

		bool needsUpdate = false;
		bool neighbourNeedUpdate[4] { false };

		// Set the TextureManager (must be called before building meshes)
		void setTextureManager(const TextureManager* tm) { textureManager = tm; }

		// Release GL resources
		void releaseGL();

		void updateMesh();
		void buildMesh(); // Build both solid and water meshes
		void buildMeshData();
		void uploadMesh();

		inline const GLuint getVao() const {return VAO;}
		inline const uint getMeshVerticesSize() const {return meshVerticesSize;}
		
		inline const GLuint getWaterVao() const {return waterVAO;}
		inline const uint getWaterMeshVerticesSize() const {return waterMeshVerticesSize;}
};

#endif
