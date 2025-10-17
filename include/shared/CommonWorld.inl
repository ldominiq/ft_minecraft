
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
BlockType CommonWorld<ChunkT>::getBlockWorld(glm::ivec3 globalCoords) const
{
	int x, y, z;
	int chunkX, chunkZ;
	globalCoordsToLocalCoords(x, y, z, globalCoords.x, globalCoords.y, globalCoords.z, chunkX, chunkZ);

	auto it = chunks.find(std::make_pair(chunkX, chunkZ));
	if (it == chunks.end()) {
		return BlockType::END;
	}
	std::shared_ptr<Chunk> currChunk = it->second;
	return currChunk->getBlock(x, y, z);
}

template <typename ChunkT>
bool CommonWorld<ChunkT>::isBlockVisibleWorld(glm::ivec3 globalCoords)
{
	int x, y, z;
	int chunkX, chunkZ;
	globalCoordsToLocalCoords(x, y, z, globalCoords.x, globalCoords.y, globalCoords.z, chunkX, chunkZ);

	auto it = chunks.find(std::make_pair(chunkX, chunkZ));
	if (it == chunks.end()) {
		return false;
	}

	std::shared_ptr<Chunk> currChunk = it->second;
	return currChunk->isBlockVisible(glm::vec3(x, y ,z));
}

template <typename ChunkT>
bool CommonWorld<ChunkT>::getTargetedBlock(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir, glm::ivec3& hitBlock, glm::ivec3& faceNormal, float maxDistance)
{
    glm::ivec3 blockPos = glm::floor(rayOrigin);

    glm::vec3 deltaDist = glm::abs(glm::vec3(1.0f) / rayDir);
    glm::ivec3 step;
    glm::vec3 sideDist;

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

    while (distanceTraveled < maxDistance) {
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
        faceNormal = glm::ivec3(0);
        faceNormal[axis] = -step[axis];

		distanceTraveled = glm::min(glm::min(sideDist.x, sideDist.y), sideDist.z);

        // Check if this block exists in your world
        if (isBlockVisibleWorld(blockPos)) {
            hitBlock = blockPos;
            return true;
        }
    }

    return false;
}

template <typename ChunkT>
bool CommonWorld<ChunkT>::removeTargettedBlock(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir)
{
	glm::ivec3 blockPos, faceNormal;
	if (getTargetedBlock(rayOrigin, rayDir, blockPos, faceNormal))
	{
		setBlockWorld(blockPos, std::nullopt, BlockType::AIR);
		return true;
	}
	return false;
}

template <typename ChunkT>
void CommonWorld<ChunkT>::setTargettedBlock(const glm::vec3 &rayOrigin, const glm::vec3 &rayDir)
{
	glm::ivec3 blockPos, faceNormal;
	if (getTargetedBlock(rayOrigin, rayDir, blockPos, faceNormal))
	{
		for (auto &entity : entities)
			if (entity->entityCollidesWithBlock(blockPos + faceNormal)) return ; // TODO : only check collision with living entities
		setBlockWorld(blockPos, faceNormal, BlockType::WATER);
	}
}
