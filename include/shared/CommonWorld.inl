
#include "CommonWorld.hpp"

template <typename ChunkT>
std::shared_ptr<ChunkT> CommonWorld<ChunkT>::getChunk(int chunkX, int chunkZ) {
    const ChunkPos key = Chunk::toKey(chunkX, chunkZ);
    auto it = chunks.find(key);
    if (it == chunks.end())
        return nullptr;
    return it->second;
}

template <typename ChunkT>
void CommonWorld<ChunkT>::globalCoordsToLocalCoords(int &x, int &y, int &z, int globalX, int globalY, int globalZ, int &chunkX, int &chunkZ) const
{
	x = (globalX % Chunk::WIDTH + Chunk::WIDTH) % Chunk::WIDTH;
	z = (globalZ % Chunk::DEPTH + Chunk::DEPTH) % Chunk::DEPTH;
	y = globalY;

	chunkX = globalX / Chunk::WIDTH;
	if (globalX < 0 && globalX % Chunk::WIDTH != 0)
		chunkX--;

	chunkZ = globalZ / Chunk::DEPTH;
	if (globalZ < 0 && globalZ % Chunk::DEPTH != 0)
		chunkZ--;
}

template <typename ChunkT>
std::shared_ptr<ChunkT> CommonWorld<ChunkT>::resolveTarget(glm::ivec3 globalCoords,
                                                            std::optional<glm::ivec3> faceNormal,
                                                            int& x, int& y, int& z) const
{
	if (faceNormal.has_value())
		globalCoords += *faceNormal;

	int chunkX, chunkZ;
	globalCoordsToLocalCoords(x, y, z, globalCoords.x, globalCoords.y, globalCoords.z, chunkX, chunkZ);

	auto it = chunks.find(std::make_pair(chunkX, chunkZ));
	if (it == chunks.end())
		return nullptr;
	return it->second;
}

template <typename ChunkT>
BlockType CommonWorld<ChunkT>::getBlockWorld(glm::ivec3 globalCoords) const
{
	int x, y, z;
	auto chunk = resolveTarget(globalCoords, std::nullopt, x, y, z);
	if (!chunk)
		return BlockType::END;
	return chunk->getBlock(x, y, z);
}

template <typename ChunkT>
uint8_t CommonWorld<ChunkT>::getSkyLightWorld(glm::ivec3 globalCoords) const
{
	int x, y, z;
	auto chunk = resolveTarget(globalCoords, std::nullopt, x, y, z);
	// Chunk not loaded -> assume sunlit (15), same default Chunk::getSkyLight
	// returns when its own skyLight grid hasn't been computed yet. Entities
	// streaming into unloaded chunks should not flash to black.
	if (!chunk)
		return 15;
	return chunk->getSkyLight(x, y, z);
}

template <typename ChunkT>
bool CommonWorld<ChunkT>::isBlockVisibleWorld(glm::ivec3 globalCoords)
{
	int x, y, z;
	auto chunk = resolveTarget(globalCoords, std::nullopt, x, y, z);
	if (!chunk)
		return false;
	return chunk->isBlockVisible(glm::vec3(x, y, z));
}

template <typename ChunkT>
bool CommonWorld<ChunkT>::rayIntersectsAABB(const glm::vec3& rayOrigin, const glm::vec3& rayDir, const AABB& box, float maxDistance, float& outT)
{
	//check that raydir isn't 0 to avoid division by 0;
	const float eps = 1e-6f;
	glm::vec3 safeDir(
		rayDir.x > 0.0f ? glm::max(rayDir.x, eps) : (rayDir.x < 0.0f ? glm::min(rayDir.x, -eps) : eps),
		rayDir.y > 0.0f ? glm::max(rayDir.y, eps) : (rayDir.y < 0.0f ? glm::min(rayDir.y, -eps) : eps),
		rayDir.z > 0.0f ? glm::max(rayDir.z, eps) : (rayDir.z < 0.0f ? glm::min(rayDir.z, -eps) : eps)
	);
	glm::vec3 invDir = 1.0f / safeDir;

    // box.min/max are dvec3 now (precise at large coords). Do the subtraction
    // in double so the camera-relative ray distances stay accurate, then
    // narrow to float for the per-axis t comparisons.
    const glm::dvec3 rayOriginD(rayOrigin);
    glm::vec3 t0 = glm::vec3(box.min - rayOriginD) * invDir;
    glm::vec3 t1 = glm::vec3(box.max - rayOriginD) * invDir;

    glm::vec3 tmin = glm::min(t0, t1);
    glm::vec3 tmax = glm::max(t0, t1);

    float entry = glm::max(glm::max(tmin.x, tmin.y), tmin.z);
    float exit  = glm::min(glm::min(tmax.x, tmax.y), tmax.z);

    if (exit < 0.0f || entry > exit)
        return false;

    if (entry > maxDistance)
        return false;

    outT = entry;
    return true;
}

template <typename ChunkT>
bool CommonWorld<ChunkT>::findClosestEntityHit(const LivingEntity& src, float maxDistance, LivingEntity*& outEntity, float& outT)
{
    outT = maxDistance;
    outEntity = nullptr;

    glm::vec3 rayOrigin = src.getPosition() + glm::vec3(0, src.getEyesHeight(), 0);
    glm::vec3 rayDir = src.Front;

    for (const auto& e : livingEntities) {
        AABB box = e->constructAABB(e->getPosition());

        float t;
        if (rayIntersectsAABB(rayOrigin, rayDir, box, maxDistance, t)) {
			if (e.get() == &src) continue; //don't hit yourself
            if (t < outT) {
                outT = t;
                outEntity = e.get();
            }
        }
    }

    return outEntity != nullptr;
}

template <typename ChunkT>
TargetType CommonWorld<ChunkT>::getTarget(const LivingEntity& src, glm::ivec3& hitBlock, glm::ivec3& faceNormal, LivingEntity*& livingEntity, bool ignoreLiquids,
                                          float blockMaxDistance, float entityMaxDistance)
{
	bool entityHit = false;
	float entityT = entityMaxDistance;

	glm::vec3 rayOrigin = src.getPosition() + glm::vec3(0, src.getEyesHeight(), 0);
	glm::vec3 rayDir = src.Front;

	entityHit = findClosestEntityHit(src, entityMaxDistance, livingEntity, entityT);

    glm::ivec3 blockPos = glm::floor(rayOrigin);

    glm::vec3 deltaDist = glm::abs(glm::vec3(1.0f) / rayDir);
    glm::ivec3 step{};
    glm::vec3 sideDist{};

    for (int i = 0; i < 3; ++i) {
        if (rayDir[i] < 0) {
            step[i] = -1;
            sideDist[i] = (rayOrigin[i] - blockPos[i]) * deltaDist[i];
        } else {
            step[i] = 1;
            sideDist[i] = (blockPos[i] + 1.0f - rayOrigin[i]) * deltaDist[i];
        }
    }

    float distanceTraveled = 0.0f;
    glm::ivec3 prevBlock = blockPos;

    while (distanceTraveled < blockMaxDistance) {
        int axis;
        if (sideDist.x < sideDist.y) {
            if (sideDist.x < sideDist.z) axis = 0;
            else                         axis = 2;
        } else {
            if (sideDist.y < sideDist.z) axis = 1;
            else                         axis = 2;
        }

        blockPos[axis] += step[axis];
        sideDist[axis] += deltaDist[axis];

        // Track face direction
        faceNormal = glm::ivec3{};
        faceNormal[axis] = -step[axis];

		distanceTraveled = glm::min(glm::min(sideDist.x, sideDist.y), sideDist.z);

		if (entityHit && distanceTraveled >= entityT)
			return TargetType::LivingEntity;

        // Check if this block exists in your world
        if (isBlockVisibleWorld(blockPos)) {
            hitBlock = blockPos;
            return TargetType::Block;
        }

		if (ignoreLiquids == false && isBlockLiquid(getBlockWorld(blockPos)))
		{
			hitBlock = blockPos;
			return TargetType::Block;
		}
    }

    // Block ray ran past blockMaxDistance; fall back to entity hit if any.
    if (entityHit)
        return TargetType::LivingEntity;
    return TargetType::None;
}

// Check if camera/player is underwater
template <typename ChunkT>
bool CommonWorld<ChunkT>::isUnderwater(const glm::dvec3 &position) const
{
	// Floor in double space — at large world coords (~1e6) float can't
	// represent 1-block increments, which mis-floors near block boundaries.
	glm::ivec3 blockPos = glm::ivec3(glm::floor(position));

	const BlockType block = getBlockWorld(blockPos);
	if (block == BlockType::WATER)
		return true;

	// Handling surface edge case
	const glm::ivec3 blockBelow = blockPos - glm::ivec3(0, 1, 0);
	BlockType blockBelowType = getBlockWorld(blockBelow);

	if (blockBelowType == BlockType::WATER) {
		double distanceAboveWater = position.y - glm::floor(position.y);

		if (distanceAboveWater < 0.15)
			return true;
	}

	return false;
}

// template <typename ChunkT>
// bool CommonWorld<ChunkT>::removeTargettedBlock(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, BlockType &dropped)
// {
// 	glm::ivec3 blockPos, faceNormal;
// 	if (getTarget(rayOrigin, rayDir, blockPos, faceNormal) == TargetType::Block)
// 	{
// 		dropped = getBlockWorld(blockPos);
// 		setBlockWorld(blockPos, std::nullopt, BlockType::AIR);
// 		return true;
// 	}
// 	return false;
// }

// template <typename ChunkT>
// bool CommonWorld<ChunkT>::setTargettedBlock(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, const BlockType block)
// {
// 	glm::ivec3 blockPos, faceNormal;
// 	if (getTarget(rayOrigin, rayDir, blockPos, faceNormal) == TargetType::Block)
// 	{
// 		for (auto &entity : livingEntities)
// 			if (entity->entityCollidesWithBlock(blockPos + faceNormal)) return false; //only checks collision with living entities
// 		if (setBlockWorld(blockPos, faceNormal, block))
// 			return true;
// 	}
// 	return false;
// }
