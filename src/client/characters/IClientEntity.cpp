
#include "IClientEntity.hpp"

IClientEntity::IClientEntity(const glm::vec3 &position, float yaw, entityID ID): LivingEntity(position, yaw, ID), Character()
{
}
