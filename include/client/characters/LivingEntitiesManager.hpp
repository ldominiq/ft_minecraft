#ifndef LIVING_ENTITIES_MANAGER_HPP
#define LIVING_ENTITIES_MANAGER_HPP

#include <memory>
#include <vector>
#include <glm/glm.hpp>

#include "IClientEntity.hpp"
#include "HitboxRenderer.hpp"
#include "SkinManager.hpp"

class ICommonWorld;

//actual manager and completely different from ItemPropEntityManager... ff
class LivingEntitiesManager
{
	Shader characterShader; // Should probably reuse another one but it is what it is
	std::vector<std::weak_ptr<IClientEntity>> characters;
	HitboxRenderer hbRenderer;
	SkinManager skinManager;

	public:

		LivingEntitiesManager();
		~LivingEntitiesManager();

		void add(std::weak_ptr<IClientEntity> character);
		// `world` is used to sample the chunk sky-light grid at each entity's
		// position so mobs/players darken in caves like terrain does
		void draw(const glm::mat4 &projection, const glm::mat4 &view,
				  const glm::dvec3& eyePos, const float deltatima,
				  const ICommonWorld* world);

		// Exposed so App.cpp can call lighting->uploadLightingUniforms/uploadCSMUniforms
		// on the entity shader before draw() — entity_lighting.glsl uses the same
		// uniform names as lighting.frag so nothing else has to change.
		Shader& getShader() { return characterShader; }

		// Debug toggle for the per-entity AABB outline. Off by default
		bool showHitboxes = false;
};

#endif