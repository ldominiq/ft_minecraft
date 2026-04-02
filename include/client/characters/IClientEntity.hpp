#ifndef I_CLIENT_ENTITY_HPP
#define I_CLIENT_ENTITY_HPP

#include "LivingEntity.hpp"
#include "Character.hpp"

class IClientEntity : public virtual LivingEntity, public virtual Character {

	public:
		IClientEntity(const glm::vec3 &position, float yaw, entityID ID);
		virtual ~IClientEntity() = default;

		// Set by Camera each frame for the local player in third-person so the
		// mesh position matches the interpolated camera target instead of the
		// raw physics position, which prevents the shake.
		glm::vec3 renderPos;
		bool hasRenderPos = false;
};

#endif