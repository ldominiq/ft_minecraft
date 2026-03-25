#ifndef I_CLIENT_ENTITY_HPP
#define I_CLIENT_ENTITY_HPP

#include "LivingEntity.hpp"
#include "Character.hpp"

class IClientEntity : public virtual LivingEntity, public virtual Character {

	public:
		IClientEntity(const glm::vec3 &position, float yaw, entityID ID);
		virtual ~IClientEntity() = default;
};

#endif