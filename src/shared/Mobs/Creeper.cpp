
#include "Creeper.hpp"

Creeper::Creeper(const glm::vec3 &position):	LivingEntity(position)
{
	type = CREEPER;

	this->entityWidth = 0.6f;
	this->entityHeight = 2.0f;
}

Creeper::Creeper(const glm::vec3 &position, float yaw, entityID ID): LivingEntity(position, yaw ,ID)
{
	type = CREEPER;

	this->entityWidth = 0.6f;
	this->entityHeight = 2.0f;
}

glm::vec3 Creeper::getDesiredMove(const std::vector<std::shared_ptr<Entity>> &players)
{
    glm::vec3 moveVec = glm::vec3(0.0f);

    if (players.empty())
        return moveVec;

    // Find the closest player
    float closestDistSq = std::numeric_limits<float>::max();
    glm::vec3 closestPlayerPos = this->position;
    bool foundPlayer = false;

    const float DETECTION_RANGE = 16.0f; // blocks
    const float DETECTION_RANGE_SQ = DETECTION_RANGE * DETECTION_RANGE;
    const float STOP_DISTANCE = 1.0f; // Stop when this close to player
    const float STOP_DISTANCE_SQ = STOP_DISTANCE * STOP_DISTANCE;

    for (const auto &player : players)
    {
        glm::vec3 playerPos = player->getPosition();
        glm::vec3 delta = playerPos - this->position;
        float distSq = glm::dot(delta, delta);

        // Check if within detection range and closer than previous closest
        if (distSq < DETECTION_RANGE_SQ && distSq < closestDistSq)
        {
            closestDistSq = distSq;
            closestPlayerPos = playerPos;
            foundPlayer = true;
        }
    }

    // Only move if a player was found within range and outside stop distance
    if (foundPlayer && closestDistSq > STOP_DISTANCE_SQ)
    {
        // Move towards the closest player (horizontal movement only)
        glm::vec3 direction = closestPlayerPos - this->position;
        direction.y = 0.0f; // Ignore vertical component for movement direction
        
        float horizontalDist = glm::length(direction);
        if (horizontalDist > 0.01f) // Check if direction is non-zero
        {
            direction = glm::normalize(direction);
            
            // Movement speed in blocks/second, scaled to per-tick delta
            const float DELTA_TIME = 1.0f / 20.0f; // Server runs at 20 TPS
            
            // Return position delta for this tick (not velocity)
            moveVec = direction * WALKING_SPEED * DELTA_TIME;
            
            // Update yaw to face the player
            this->yaw = glm::degrees(atan2(direction.z, direction.x));
        }
		// TODO: add some pathfinding to avoid obstacles maybe?
		// TODO: Jump over small obstacles
		// TODO: Add collision checks between creeper and player
		
    }

    return moveVec;
}

void Creeper::calculateNewPosition(const ICommonWorld &world, const std::vector<std::shared_ptr<Entity>> &players)
{
	// jump = true;
	doJump(world);
	glm::vec3 desiredMove = getDesiredMove(players);
	this->calculateNewXZPosition(world, desiredMove);
	this->calculateNewYPosition(world);
	// jump = false;
}