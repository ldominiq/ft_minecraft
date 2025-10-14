
#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <vector>
#include <unordered_set>
#include <future>

#include <glm/glm.hpp>
#include <memory>
#include <zstd.h>
#include <optional>

#include "Shader.hpp"
#include "ChunkRenderer.hpp"
#include "Protocol.hpp"
#include "CommonWorld.hpp"

// previously half of World

struct chunkData {
	uint32_t compressedSize;
	uint32_t uncompressedSize;
	std::vector<uint8_t> chunkBuffer;
	// size_t receivedBytes = 0;
}; 

class Renderer final : public CommonWorld<ChunkRenderer> {

	int loadRadius = 16;
	int unloadRadius = loadRadius + 16;

	std::unordered_map<ChunkPos, chunkData> chunksData; //building chunk
	std::unordered_set<ChunkPos> chunksToBuild; // chunk to build and upload mesh
	std::vector<std::weak_ptr<ChunkRenderer>> renderedChunks;

	void linkNeighbors(int chunkX, int chunkZ, std::shared_ptr<ChunkRenderer> &chunk);

	public:
		std::vector<std::weak_ptr<ChunkRenderer>> getRenderedChunks();

		void render(const std::shared_ptr<Shader> &shaderProgram) const ;
		void renderWater() const;

		// Get or set the current chunk load radius.  The radius determines how
		// many chunks around the camera are loaded.  Values below 1 are clamped.
		int getLoadRadius() const { return loadRadius; }
		void setLoadRadius(int radius) { loadRadius = std::max(1, radius); }

		void buildChunks();
		void updateChunk(const NetModifiedBlockData &pkt);
		void organizeChunks(const std::pair<int, int> pos);
    	void draw(const std::shared_ptr<Shader> &shaderProgram, const GLuint &VAO, const uint &meshVerticesSize) const; // Draw the chunk using the given shader program

		void prepareChunk(const NetChunkHeader& pkt);
		void receiveChunk(const NetChunkData& pkt);

		void setBlockWorld(glm::ivec3 globalCoords, std::optional<glm::ivec3> faceNormal, BlockType type) override;

		inline size_t getVisibleChunkCount() const {
			return renderedChunks.size();
		}
};

#endif