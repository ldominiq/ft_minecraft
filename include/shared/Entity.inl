
#include "Entity.hpp"
#include "CommonWorld.hpp"

template <typename ChunkT>
Entity<ChunkT>::Entity(glm::vec3 position): position(position) {}

template <typename ChunkT>
Entity<ChunkT>::~Entity() {}

// Build a current-player AABB (min at feet)
template <typename ChunkT>
AABB Entity<ChunkT>::constructAABB(const glm::vec3 &pos) {
	glm::vec3 mn(pos.x - entityWidth * 0.5f, pos.y,				pos.z - entityWidth * 0.5f);
	glm::vec3 mx(pos.x + entityWidth * 0.5f, pos.y + entityHeight, pos.z + entityWidth * 0.5f);
	return AABB(mn, mx);
};

template <typename ChunkT>
bool Entity<ChunkT>::aabbCollidesWithWorld(const AABB &box, const CommonWorld<ChunkT> &world) {
    // compute block search bounds (floor)
    int minX = (int)std::floor(box.min.x + EPS);
    int maxX = (int)std::floor(box.max.x - EPS);
    int minY = (int)std::floor(box.min.y + EPS);
    int maxY = (int)std::floor(box.max.y - EPS);
    int minZ = (int)std::floor(box.min.z + EPS);
    int maxZ = (int)std::floor(box.max.z - EPS);

    for (int x = minX; x <= maxX; ++x)
	for (int y = minY; y <= maxY; ++y)
	for (int z = minZ; z <= maxZ; ++z) {
		BlockType b = world.getBlockWorld({x, y, z});
		if (isSolidBlock(b)) {
			// block occupies AABB {x..x+1, y..y+1, z..z+1} -> any overlap is collision
			// we already limited the loop to candidate blocks, so we can early return
			return true;
		}
	}

    return false;
}

template <typename ChunkT>
void Entity<ChunkT>::calculateNewXZPosition(const CommonWorld<ChunkT> &world, glm::vec3 &desiredMove)
{
    glm::vec3 newPos = position;
    AABB currentBox = constructAABB(position);

    // X axis
    if (std::abs(desiredMove.x) > EPS) {
        float dx = desiredMove.x;
        AABB movedX = currentBox.movedBy(dx, 0.0f, 0.0f);
        if (!aabbCollidesWithWorld(movedX, world)) {
            newPos.x += dx;
            currentBox = constructAABB(newPos);
		}
    }

    // Z axis
    if (std::abs(desiredMove.z) > EPS) {
        float dz = desiredMove.z;
        AABB movedZ = currentBox.movedBy(0.0f, 0.0f, dz);
        if (!aabbCollidesWithWorld(movedZ, world)) {
            newPos.z += dz;
            currentBox = constructAABB(newPos);
		}
    }

	position = newPos;
}

template <typename ChunkT>
void Entity<ChunkT>::calculateNewYPosition(const CommonWorld<ChunkT> &world)
{
	glm::vec3 newPos = position;
	AABB currentBox = constructAABB(position);

	// attempt Y movement
	float dy = verticalVelocity;
	if (std::abs(dy) > EPS) {
		AABB movedY = constructAABB(newPos).movedBy(0.0f, dy, 0.0f);
		if (!aabbCollidesWithWorld(movedY, world)) {
			newPos.y += dy;
		} else {
			// collision on Y: either hit head (dy>0) or land (dy<0)
			if (dy > 0.0f) {
				// head collision: find nearest block above to snap below
				int startY = (int)std::floor(currentBox.max.y + EPS);
				int endY = (int)std::floor(currentBox.max.y + dy + 1.0f);
				bool stopped = false;
				for (int by = startY; by <= endY && !stopped; ++by) {
					// check blocks at by that overlap horizontal footprint
					int minBX = (int)std::floor(currentBox.min.x + EPS);
					int maxBX = (int)std::floor(currentBox.max.x - EPS);
					int minBZ = (int)std::floor(currentBox.min.z + EPS);
					int maxBZ = (int)std::floor(currentBox.max.z - EPS);
					for (int bx = minBX; bx <= maxBX && !stopped; ++bx) {
						for (int bz = minBZ; bz <= maxBZ && !stopped; ++bz) {
							if (isSolidBlock(world.getBlockWorld({bx, by, bz}))) {
								float headBefore = currentBox.max.y;
								float headAfter  = currentBox.max.y + dy;

								// only stop if we were below the block and tried to enter it
								if (headBefore <= by + EPS && headAfter > by - EPS) {
									newPos.y = (float)by - entityHeight - EPS; // snap below ceiling
									verticalVelocity = 0.0f;
									stopped = true;
								}
							}
						}
					}
				}
				if (!stopped) verticalVelocity = 0.0f; // fallback
			} else {
				// falling -> landed: find the highest solid block we hit and snap on top
				int fromY = (int)std::floor(currentBox.min.y + dy - 1.0f); // lower bound after fall
				int toY = (int)std::floor(currentBox.min.y + EPS);       // current foot block
				bool landed = false;
				for (int by = toY; by >= fromY && !landed; --by) {
					int minBX = (int)std::floor(currentBox.min.x + EPS);
					int maxBX = (int)std::floor(currentBox.max.x - EPS);
					int minBZ = (int)std::floor(currentBox.min.z + EPS);
					int maxBZ = (int)std::floor(currentBox.max.z - EPS);
					for (int bx = minBX; bx <= maxBX && !landed; ++bx) {
						for (int bz = minBZ; bz <= maxBZ && !landed; ++bz) {
							if (isSolidBlock(world.getBlockWorld({bx, by, bz}))) {
								float feetBefore = currentBox.min.y;
								float feetAfter  = currentBox.min.y + dy;

								// only snap if we were above the block and are now moving into it
								if (feetBefore >= by + 1.0f - EPS && feetAfter < by + 1.0f) {
									newPos.y = (float)by + 1.0f + EPS; // snap to block top
									verticalVelocity = 0.0f;
									onGround = true;
									landed = true;
								}
							}
						}
					}
				}
				if (!landed) {
					// fallback: keep current vertical (should be rare)
					newPos.y += dy;
				}
			}
		}
	}

	// apply gravity
	verticalVelocity -= GRAVITY; //gravity
	verticalVelocity *= DRAG;
	if (std::abs(verticalVelocity) < 0.003) verticalVelocity = 0;

	// Apply final position
	position = newPos;
}
