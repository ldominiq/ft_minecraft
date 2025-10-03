
#include "ItemEntity.hpp"

//server
ItemEntity::ItemEntity(glm::vec3 position, float yaw, BlockType ID):	Entity(glm::vec3(position.x, position.y - 0.2f, position.z))
{
	item = ID;
	this->yaw = yaw;

	entityHeight = 0.2f;
	entityWidth = 0.2f;

    // Random small horizontal velocity
    float angle = glm::radians(yaw) + (float(rand()) / RAND_MAX - 0.5f) * 0.2f; // small random offset
    float speed = 0.1f + (float(rand()) / RAND_MAX) * 0.05f; // 0.1–0.15

    velocity.x = std::cos(angle) * speed;
    velocity.z = std::sin(angle) * speed;

    // Pop upwards a bit
    velocity.y = 0.2f + (float(rand()) / RAND_MAX) * 0.1f;
}

//client
ItemEntity::ItemEntity(glm::vec3 position, BlockType ID, entityID entityID): Entity(position, entityID) 
{
	item = ID;

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