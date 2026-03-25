
#include "ClientPlayer.hpp"

ClientPlayer::ClientPlayer(const glm::vec3 &position, float yaw, entityID ID) : PlayerMovement(position, yaw, ID), IClientEntity(position,yaw,ID), Character(), LivingEntity(position, yaw, ID)
{
	createCharacterAt(position, entityHeight);
}