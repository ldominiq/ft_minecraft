
#include "Creeper.hpp"

Creeper::Creeper(const glm::vec3 &position):	LivingEntity(position)
{
	type = CREEPER;

	this->entityWidth = 0.6f;
	this->entityHeight = 20.0f;

	SAFE_FALL_DISTANCE = 20;
}

Creeper::Creeper(const glm::vec3 &position, float yaw, entityID ID): LivingEntity(position, yaw ,ID)
{
	type = CREEPER;

	this->entityWidth = 0.6f;
	this->entityHeight = 20.0f;

	SAFE_FALL_DISTANCE = 20;
}

glm::vec3 Creeper::getDesiredMove()
{
	return LivingEntity::getDesiredMove();
}

void Creeper::calculateNewPosition(const ICommonWorld &world)
{
	jump = false;
	doJump(world);
	glm::vec3 desiredMove = getDesiredMove();
	this->calculateNewXZPosition(world, desiredMove);
	this->calculateNewYPosition(world);
	jump = false;
}