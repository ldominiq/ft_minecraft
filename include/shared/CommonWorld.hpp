
#ifndef COMMON_WORLD_HPP
#define COMMON_WORLD_HPP

#include <unordered_map>
#include <optional>

#include "Chunk.hpp"

template <typename WorldT>
class LivingEntity;

class ICommonWorld {
public:
    virtual void something() = 0;
    virtual ~ICommonWorld() = default;
};

template <typename ChunkT>
class CommonWorld : public ICommonWorld {
public:
    void something() override {
        // implementation using ChunkT
    }
};

template <typename ChunkT>
class CommonWorld : ICommonWorld{

	protected:
		std::unordered_map<ChunkPos, std::shared_ptr<ChunkT>> chunks;

	public:
		void globalCoordsToLocalCoords(int &x, int &y, int &z, int globalX, int globalY, int globalZ, int &chunkX, int &chunkZ) const;
		std::shared_ptr<ChunkT> getChunk(int chunkX, int chunkZ);
		BlockType getBlockWorld(glm::ivec3 globalCoords) const; //unused for now
		virtual void setBlockWorld(glm::ivec3 globalCoords, std::optional<glm::ivec3> faceNormal, BlockType type) = 0;
		bool isBlockVisibleWorld(glm::ivec3 globalCoords);

		bool getTargetedBlock(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, glm::ivec3& hitBlock, glm::ivec3& faceNormal, float maxDistance = 100);
		void removeTargettedBlock(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir);
		void setTargettedBlock(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir);

		// Return the total number of chunks currently loaded in the world (in memory).
		inline std::size_t getTotalChunkCount() const {
			return chunks.size();
		}

		std::vector<std::shared_ptr<LivingEntity<ChunkT>>> livingEntities;
};

#include "CommonWorld.inl"

#endif