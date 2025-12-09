
#include "Creeper.hpp"
#include "CommonWorld.hpp"

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

glm::vec3 Creeper::getDesiredMove(const ICommonWorld &world, const std::vector<std::shared_ptr<Entity>> &players)
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
    const float STOP_DISTANCE = 2.0f; // Stop when this close to player
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
    // if (foundPlayer && closestDistSq > STOP_DISTANCE_SQ)
    // {
    //     // Move towards the closest player (horizontal movement only)
    //     glm::vec3 direction = closestPlayerPos - this->position;
    //     direction.y = 0.0f; // Ignore vertical component for movement direction
        
    //     float horizontalDist = glm::length(direction);
    //     if (horizontalDist > 0.01f) // Check if direction is non-zero
    //     {
    //         direction = glm::normalize(direction);
            
    //         // Return position delta for this tick (not velocity)
    //         moveVec = direction * WALKING_SPEED * DELTA_TIME;
            
    //         // Update yaw to face the player
    //         this->yaw = glm::degrees(atan2(direction.z, direction.x));

    //         checkObstacleAndJump(world, direction);
    //     }
	// 	// TODO: add some pathfinding to avoid obstacles maybe?
	// 	// TODO: Add collision checks between creeper and player
	// 	// TODO: Add collision checks between mobs
    //     // TODO: Add explosion logic when close enough to player
        
    // } else {
        // No player nearby - wander randomly
        moveVec = getWanderMove(world);
    // }

    return moveVec;
}

glm::vec3 Creeper::getWanderMove(const ICommonWorld &world)
{
    const float WANDER_SPEED = WALKING_SPEED * 0.5f; // Slower when wandering
    const float MIN_WANDER_COOLDOWN = 6.0f; // Minimum seconds between wander direction changes
    const float MAX_WANDER_COOLDOWN = 20.0f; // Maximum seconds between wander direction changes
    const float WANDER_DURATION = 3.0f; // Seconds to walk in wander direction

    wanderCooldown -= DELTA_TIME;
    
    // If cooldown expired, pick new random direction
    if (wanderCooldown <= 0.0f)
    {
        // Random wait time before next wander
        float randomWait = MIN_WANDER_COOLDOWN + 
            static_cast<float>(rand()) / RAND_MAX * (MAX_WANDER_COOLDOWN - MIN_WANDER_COOLDOWN);
        wanderCooldown = randomWait;
        wanderDuration = WANDER_DURATION;
        
        // Random angle in radians
        float randomAngle = static_cast<float>(rand()) / RAND_MAX * 2.0f * M_PI;
        wanderDirection = glm::vec3(cos(randomAngle), 0.0f, sin(randomAngle));
        this->yaw = glm::degrees(randomAngle);
    }
    
    // Execute wander move if duration hasn't expired
    if (wanderDuration > 0.0f)
    {
        wanderDuration -= DELTA_TIME;
        checkObstacleAndJump(world, wanderDirection);
        return wanderDirection * WANDER_SPEED * DELTA_TIME;
    }
    
    return glm::vec3(0.0f);
}

void Creeper::checkObstacleAndJump(const ICommonWorld &world, const glm::vec3 &direction)
{
    // Check blocks ahead at current height and one block up
    glm::vec3 checkPos = this->position + direction * 0.8f; // Check slightly ahead
    
    // Check if there's a solid block at foot level ahead
    glm::ivec3 blockPosAhead = glm::floor(checkPos);
    glm::ivec3 blockPosAbove = blockPosAhead + glm::ivec3(0, 1, 0);

    BlockType blockAhead = world.getBlockWorld({blockPosAhead.x, blockPosAhead.y, blockPosAhead.z});
    BlockType blockAbove = world.getBlockWorld({blockPosAbove.x, blockPosAbove.y, blockPosAbove.z});

    // If there's a block ahead at foot level but not at head level, jump
    if (blockAhead != BlockType::AIR && blockAbove == BlockType::AIR || blockAbove == BlockType::WATER)
    {
        if (this->velocity.y <= 0.0f)
        {
            this->jump = true;
        }
    }
}

void Creeper::calculateNewPosition(const ICommonWorld &world, const std::vector<std::shared_ptr<Entity>> &players)
{
	glm::vec3 desiredMove = getDesiredMove(world, players);
	doJump(world);
	this->calculateNewXZPosition(world, desiredMove);
	this->calculateNewYPosition(world);
	this->jump = false;
}