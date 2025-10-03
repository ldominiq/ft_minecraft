
#include "Entity.hpp"
#include "CommonWorld.hpp"

ItemEntityIDManager Entity::idManager;

Entity::Entity(glm::vec3 position): position(position), ID(idManager.acquire()) {}

Entity::Entity(glm::vec3 position, entityID entityID): position(position), ID(entityID) {}

Entity::~Entity()
{
	idManager.release(ID);
}

// Build a current-player AABB (min at feet)
AABB Entity::constructAABB(const glm::vec3 &pos) {
	glm::vec3 mn(pos.x - entityWidth * 0.5f, pos.y,				pos.z - entityWidth * 0.5f);
	glm::vec3 mx(pos.x + entityWidth * 0.5f, pos.y + entityHeight, pos.z + entityWidth * 0.5f);
	return AABB(mn, mx);
};

bool Entity::aabbCollidesWithWorld(const AABB &box, const ICommonWorld &world) {
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

bool Entity::entityCollidesWithBlock(const glm::vec3 blockPos) {
	glm::vec3 tmpPos = position;
	tmpPos.y = tmpPos.y - entityHeight + 0.3f; //ugly hack for player
    AABB box = constructAABB(tmpPos);
	float blockPlacementTolerance = entityHeight * 0.1f; // variable used to be able to place blocks under yourself

	//entityHeight is an ugly hack only useful for player.
    int minX = static_cast<int>(std::floor(box.min.x + EPS));
    int maxX = static_cast<int>(std::floor(box.max.x - EPS));
    int minY = static_cast<int>(std::floor(box.min.y + EPS + blockPlacementTolerance));
    int maxY = static_cast<int>(std::floor(box.max.y - EPS));
    int minZ = static_cast<int>(std::floor(box.min.z + EPS));
    int maxZ = static_cast<int>(std::floor(box.max.z - EPS));

	int bx = static_cast<int>(std::floor(blockPos.x));
	int by = static_cast<int>(std::floor(blockPos.y));
	int bz = static_cast<int>(std::floor(blockPos.z));

	for (int x = minX; x <= maxX; x++)
	for (int y = minY; y <= maxY; y++)
	for (int z = minZ; z <= maxZ; z++) {
		if (blockPos.x == x && blockPos.y == y && blockPos.z == z) return true;
	}

    return false;
}

void Entity::calculateNewXZPosition(const ICommonWorld &world, glm::vec3 &desiredMove)
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
		else velocity.x = 0.0f;
    }

    // Z axis
    if (std::abs(desiredMove.z) > EPS) {
        float dz = desiredMove.z;
        AABB movedZ = currentBox.movedBy(0.0f, 0.0f, dz);
        if (!aabbCollidesWithWorld(movedZ, world)) {
            newPos.z += dz;
            currentBox = constructAABB(newPos);
		}
		else velocity.z = 0.0f;
    }

	position = newPos;
}

void Entity::calculateNewYPosition(const ICommonWorld &world)
{
	glm::vec3 newPos = position;
	AABB currentBox = constructAABB(position);

	// attempt Y movement

	onGround = false;
	float remainingDy = verticalVelocity;
	while (std::abs(remainingDy) > 0.0f + EPS) {
		float step = remainingDy;// glm::clamp(remainingDy, -0.99f, 0.99f); // at most ~1 block per sub-step
		
		float dy = step;
		if (std::abs(dy) > EPS) {
			AABB movedY = constructAABB(newPos).movedBy(0.0f, dy, 0.0f);
			if (!aabbCollidesWithWorld(movedY, world)) {
				newPos.y += dy;
				currentBox = movedY;
			} else {
				// collision on Y: either hit head (dy>0) or land (dy<0)
				if (dy > 0.0f) {
					// head collision: find nearest block above to snap below
					int startY = (int)std::floor(currentBox.max.y);
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
									if (headBefore <= by && headAfter > by) {
										newPos.y = (float)by - entityHeight; // snap below ceiling
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
					int toY = (int)std::floor(currentBox.min.y);       // current foot block
					bool landed = false;
					for (int by = toY; by >= fromY && !landed; --by) {
						int minBX = (int)std::floor(currentBox.min.x + EPS);
						int maxBX = (int)std::floor(currentBox.max.x - EPS);
						int minBZ = (int)std::floor(currentBox.min.z + EPS);
						int maxBZ = (int)std::floor(currentBox.max.z - EPS);
						for (int bx = minBX; bx <= maxBX && !landed; ++bx) {
							for (int bz = minBZ; bz <= maxBZ && !landed; ++bz) { // this is getting done twice if .x = .z . meh performance
								if (isSolidBlock(world.getBlockWorld({bx, by, bz}))) {
									float feetBefore = currentBox.min.y;
									float feetAfter  = currentBox.min.y + dy;

									// only snap if we were above the block and are now moving into it
									if (entityHeight < 1.0f && feetBefore >= by + entityHeight && feetAfter < by + entityHeight) /// I don't understand this but it.. works? idk it's weird
									{
										newPos.y = (float)by + 1.0f; // snap to block top
										verticalVelocity = 0.0f;
										onGround = true;
										landed = true;
									}
									else if (feetBefore >= by + 1.0f && feetAfter < by + 1.0f) {
										newPos.y = (float)by + 1.0f; // snap to block top
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
		remainingDy -= step;
	}

	// apply gravity
	verticalVelocity -= GRAVITY; //gravity
	verticalVelocity *= DRAG;
	if (std::abs(verticalVelocity) < 0.003 || onGround) verticalVelocity = 0;

	// Apply final position
	position = newPos;
}

void Entity::calculateNewPosition(const ICommonWorld &world)
{
	// TODO : return early if block stopped. Same as player calculateNewPosition TODO.

	glm::vec3 desiredPos = getDesiredMove();
	calculateNewXZPosition(world, desiredPos);
	calculateNewYPosition(world);
}
