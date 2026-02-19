
#include "ItemEntity.hpp"

//server
ItemEntity::ItemEntity(const glm::vec3 &position, float yaw, ItemType type, int32_t spawnTick, bool isLaunched):	Entity(position), spawnTick(spawnTick)
{
	this->type = type;
	this->yaw = yaw;

	entityHeight = 0.2f;
	entityWidth = 0.2f;

	if (isLaunched)
	{
		float angle = glm::radians(yaw) + (float(rand()) / RAND_MAX - 0.5f) * 0.2f; // small random offset
		float speed = 0.1f + (float(rand()) / RAND_MAX) * 0.05f; // 0.1–0.15
		velocity.x = std::cos(angle) * speed;
		velocity.z = std::sin(angle) * speed;
		velocity.y = 0.2f + (float(rand()) / RAND_MAX) * 0.1f;	// Pop upwards a bit
	}    
}

//client
ItemEntity::ItemEntity(const glm::vec3 &position, float yaw, ItemType type, entityID ID): Entity(position, yaw, ID) 
{
	this->type = type;

	entityHeight = 0.2f;
	entityWidth = 0.2f;
}

ItemEntity::~ItemEntity() {}

glm::vec3 ItemEntity::getDesiredMove()
{
	float slipperiness = SM_AIRBORNE;
	if (onGround) slipperiness = SM_DEFAULT;

    velocity *= DRAG * slipperiness;
    return velocity;
}