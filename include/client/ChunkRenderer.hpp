
#ifndef CHUNK_RENDERER_HPP
#define CHUNK_RENDERER_HPP

#include <glad/glad.h>
#include <cstring>
#include <array>

#include "GLFW/glfw3.h"
#include "Chunk.hpp"
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

	// Per-column heightmap: highest opaque block Y+1 for sky-light determination.
	// Indexed as heightMap[x * DEPTH + z].  Built during buildMeshData().
	std::array<int, WIDTH * DEPTH> heightMap{};

	void computeHeightMap(const std::vector<BlockType>& blockTypeVector);

	// glm::vec2 getTextureOffset(const BlockType type, const int face);
    void addFace(int x, int y, int z, int face); // Add a face to the mesh vertices (solid blocks)
	void addWaterFace(int x, int y, int z, int face); // Add a face to water mesh

	public:

		ChunkRenderer(std::istream& in);
		~ChunkRenderer();

		bool needsUpdate = false;
		bool neighbourNeedUpdate[4] { false };

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
