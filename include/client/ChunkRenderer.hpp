
#ifndef CHUNK_RENDERER_HPP
#define CHUNK_RENDERER_HPP

#include <glad/glad.h>
#include "Chunk.hpp"

class ChunkRenderer : public Chunk {

    GLuint VAO = 0;
    GLuint VBO = 0;
	uint meshVerticesSize;
    std::vector<float> meshVertices; // Vertices for the mesh

	glm::vec2 getTextureOffset(const BlockType type, const int face);
    void addFace(int x, int y, int z, int face); // Add a face to the mesh vertices

	public:

		ChunkRenderer(std::istream& in);
		~ChunkRenderer();

		const int ATLAS_COLS = 10;
		const int ATLAS_ROWS = 1;

		// Release GL resources
		void releaseGL();

		void setAdjacentChunks(int direction, std::shared_ptr<ChunkRenderer> &chunk);
		bool hasAllAdjacentChunkLoaded() const;

		void buildMesh(); // Build the mesh for Renderer
		void buildMeshData();
		void uploadMesh();

		// inline const std::pair<int32_t, int32_t> getCoords() const {return std::make_pair(originX, originZ);} //unused
		inline const GLuint getVao() const {return VAO;}
		inline const uint getMeshVerticesSize() const {return meshVerticesSize;}
};

#endif
