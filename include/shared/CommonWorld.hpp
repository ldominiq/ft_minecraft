
#ifndef COMMON_WORLD_HPP
#define COMMON_WORLD_HPP

#include <unordered_map>
#include <optional>

#include "Chunk.hpp"
#include "ItemEntity.hpp"
#include "LivingEntity.hpp"
#include "Item.hpp"

enum class TargetType {
    None,
    Block,
    LivingEntity
};

// Player interaction reach (in blocks)
constexpr float PLAYER_BLOCK_REACH  = 4.5f;
constexpr float PLAYER_ENTITY_REACH = 3.0f;

class ICommonWorld {
public:
	virtual BlockType getBlockWorld(glm::ivec3 globalCoords) const = 0;
	virtual bool isUnderwater(const glm::dvec3 &position) const = 0;
	bool isUnderwater(const glm::vec3 &position) const { return isUnderwater(glm::dvec3(position)); }
    virtual ~ICommonWorld() = default;
};

template <typename ChunkT>
class CommonWorld : public ICommonWorld{

	protected:
		std::unordered_map<ChunkPos, std::shared_ptr<ChunkT>> chunks;

		// Resolve a global position (optionally offset by faceNormal) to a chunk and local
		// coordinates. Returns nullptr if the chunk is not loaded.
		std::shared_ptr<ChunkT> resolveTarget(glm::ivec3 globalCoords,
		                                      std::optional<glm::ivec3> faceNormal,
		                                      int& x, int& y, int& z) const;

	public:
		void globalCoordsToLocalCoords(int &x, int &y, int &z, int globalX, int globalY, int globalZ, int &chunkX, int &chunkZ) const;
		std::shared_ptr<ChunkT> getChunk(int chunkX, int chunkZ);
		BlockType getBlockWorld(glm::ivec3 globalCoords) const;
		virtual bool setBlockWorld(glm::ivec3 globalCoords, std::optional<glm::ivec3> faceNormal, BlockType type) = 0;
		bool isBlockVisibleWorld(glm::ivec3 globalCoords);

		bool rayIntersectsAABB(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const AABB& box, float maxDistance, float& outT);
		bool findClosestEntityHit(const LivingEntity& src, float maxDistance, LivingEntity*& outEntity, float& outT);
		TargetType getTarget(const LivingEntity& src, glm::ivec3 &hitBlock, glm::ivec3& faceNormal, LivingEntity*& livingEntity,
		                    float blockMaxDistance  = PLAYER_BLOCK_REACH,
		                    float entityMaxDistance = PLAYER_ENTITY_REACH);
		// bool removeTargettedBlock(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, BlockType &dropped);
		// bool setTargettedBlock(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const BlockType block);

		bool isUnderwater(const glm::dvec3 &position) const override;
		using ICommonWorld::isUnderwater; // expose vec3 wrapper

		// Return the total number of chunks currently loaded in the world (in memory).
		inline std::size_t getTotalChunkCount() const {
			return chunks.size();
		}

		std::vector<std::shared_ptr<LivingEntity>> livingEntities;
		std::vector<std::shared_ptr<ItemEntity>> itemEntities;
};

#include "CommonWorld.inl"

#endif