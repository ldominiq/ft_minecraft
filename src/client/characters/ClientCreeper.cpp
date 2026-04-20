
#include "ClientCreeper.hpp"

ClientCreeper::ClientCreeper(const glm::vec3 &position, float yaw, entityID ID) : Creeper(position, yaw, ID), IClientEntity(position,yaw,ID), Character(), LivingEntity(position, yaw, ID)
{
	setPartsDimensions();
	createCharacterAt(position, entityWidth, entityHeight);
}
