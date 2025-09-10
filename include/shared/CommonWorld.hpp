
#ifndef COMMON_WORLD_HPP
#define COMMON_WORLD_HPP

#include "unordered_map"
#include "Chunk.hpp"
#include <optional>

template <typename ChunkT>
class CommonWorld {

	protected:
		std::unordered_map<ChunkPos, std::shared_ptr<ChunkT>> chunks;

	public:
		void globalCoordsToLocalCoords(int &x, int &y, int &z, int globalX, int globalY, int globalZ, int &chunkX, int &chunkZ);
		std::shared_ptr<ChunkT> getChunk(int chunkX, int chunkZ);
		BlockType getBlockWorld(glm::ivec3 globalCoords); //unused for now
		virtual void setBlockWorld(glm::ivec3 globalCoords, std::optional<glm::ivec3> faceNormal, BlockType type) = 0;
		bool isBlockVisibleWorld(glm::ivec3 globalCoords);

		bool getTargetedBlock(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, glm::ivec3& hitBlock, glm::ivec3& faceNormal, float maxDistance = 100);
		void removeTargettedBlock(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir);
		void setTargettedBlock(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir);
		
		//bool getTargetedBlock(const CPlayerInfo &player, glm::ivec3& hitBlock, glm::ivec3& faceNormal, float maxDistance = 100);
		// void removeTargettedBlock(const CPlayerInfo &player);
		// void setTargettedBlock(const CPlayerInfo &player);

		// Return the total number of chunks currently loaded in the world (in memory).
		inline std::size_t getTotalChunkCount() const {
			return chunks.size();
		}
};

#include "CommonWorld.inl"

#endif