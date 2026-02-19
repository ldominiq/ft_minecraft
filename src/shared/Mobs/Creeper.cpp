
#include "Creeper.hpp"

Creeper::Creeper(const glm::vec3 &position):	LivingEntity(position)
{
	type = CREEPER;

	this->entityWidth = 0.6f;
	this->entityHeight = 20.0f;
}

Creeper::Creeper(const glm::vec3 &position, float yaw, entityID ID): LivingEntity(position, yaw ,ID)
{
	type = CREEPER;

	this->entityWidth = 0.6f;
	this->entityHeight = 20.0f;
}

glm::vec3 Creeper::getDesiredMove()
{
	return glm::vec3(0,0,0);
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