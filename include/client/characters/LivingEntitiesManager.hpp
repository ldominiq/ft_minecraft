#ifndef LIVING_ENTITIES_MANAGER_HPP
#define LIVING_ENTITIES_MANAGER_HPP

#include <memory>
#include <vector>

#include "IClientEntity.hpp"
#include "HitboxRenderer.hpp"
#include "SkinManager.hpp"

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
       void draw(const glm::mat4 &projection, const glm::mat4 &view,
				  const glm::dvec3& eyePos, const float deltatima);
};

#endif