
#ifndef CHUNK_RENDERER_HPP
#define CHUNK_RENDERER_HPP

#include <glad/glad.h>
#include "Chunk.hpp"
#include "GLFW/glfw3.h"

class ChunkRenderer : public Chunk {

    GLuint VAO = 0;
    GLuint VBO = 0;
	uint meshVerticesSize = 0;
    std::vector<float> meshVertices; // Vertices for the mesh

	GLuint waterVAO = 0;
	GLuint waterVBO = 0;
	uint waterMeshVerticesSize = 0;
	std::vector<float> waterMeshVertices;

	glm::vec2 getTextureOffset(const BlockType type, const int face);
    void addFace(int x, int y, int z, int face); // Add a face to the mesh vertices (solid blocks)
	void addWaterFace(int x, int y, int z, int face); // Add a face to water mesh

	public:

		ChunkRenderer(std::istream& in);
		~ChunkRenderer();

		const int ATLAS_COLS = 10;
		const int ATLAS_ROWS = 1;

		// Release GL resources
		void releaseGL();

		void buildMesh(); // Build both solid and water meshes
		void buildMeshData();
		void uploadMesh();

		inline const GLuint getVao() const {return VAO;}
		inline const uint getMeshVerticesSize() const {return meshVerticesSize;}
		
		inline const GLuint getWaterVao() const {return waterVAO;}
		inline const uint getWaterMeshVerticesSize() const {return waterMeshVerticesSize;}
};

#endif
