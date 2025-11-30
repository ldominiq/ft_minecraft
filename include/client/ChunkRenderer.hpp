
#ifndef CHUNK_RENDERER_HPP
#define CHUNK_RENDERER_HPP

#include <glad/glad.h>
#include <cstring>

#include "GLFW/glfw3.h"
#include "Chunk.hpp"
#include "blockRenderingHelperFunctions.hpp"

class ChunkRenderer : public Chunk {

    GLuint VAO = 0;
    GLuint VBO = 0;
	uint meshVerticesSize = 0;
    std::vector<float> meshVertices; // Vertices for the mesh

	public:

		ChunkRenderer(std::istream& in);
		~ChunkRenderer();

		bool needsUpdate = false;
		bool neighbourNeedUpdate[4] { false };

		// Release GL resources
		void releaseGL();

		void updateMesh();
		void buildMesh(); // Build the mesh for Renderer
		void buildMeshData();
		void uploadMesh();

		inline const GLuint getVao() const {return VAO;}
		inline const uint getMeshVerticesSize() const {return meshVerticesSize;}
};

#endif
