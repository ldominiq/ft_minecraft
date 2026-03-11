
#include "ClientPlayer.hpp"

ClientPlayer::ClientPlayer(const glm::vec3 &position, float yaw, entityID ID) : LivingEntity(position, yaw, ID), Character(position), PlayerMovement(position, yaw, ID), IClientEntity(position,yaw,ID)
{
	createCharacterAt(position, entityHeight);
}