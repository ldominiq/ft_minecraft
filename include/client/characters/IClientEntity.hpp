#ifndef I_CLIENT_ENTITY_HPP
#define I_CLIENT_ENTITY_HPP

#include <string>

#include "LivingEntity.hpp"
#include "Character.hpp"

class IClientEntity : public virtual LivingEntity, public virtual Character {

	public:
		IClientEntity(const glm::vec3 &position, float yaw, entityID ID);
		virtual ~IClientEntity() = default;

		// Set by Camera each frame for the local player in third-person so the
		// mesh position matches the interpolated camera target instead of the
		// raw physics position, which prevents the shake.
        glm::dvec3 renderPos;
		bool hasRenderPos = false;

		// Name of the skin PNG to bind when rendering this entity. Empty string
		// means "no skin - fall back to per-limb colors"
		// Resolved against SkinManager at draw time.
		virtual std::string skinName() const { return ""; }
};

#endif