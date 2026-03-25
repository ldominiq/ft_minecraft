
#include "IClientEntity.hpp"

IClientEntity::IClientEntity(const glm::vec3 &position, float yaw, entityID ID): LivingEntity(position, yaw, ID), Character()
{
	characterBodyParts.character.scale = glm::scale(glm::mat4(1.0f), glm::vec3(entityWidth, entityHeight, 1));
}
