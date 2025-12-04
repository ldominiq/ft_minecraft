#ifndef LIVING_ENTITIES_MANAGER_HPP
#define LIVING_ENTITIES_MANAGER_HPP

#include <memory>
#include <vector>

#include "IClientEntity.hpp"

//actual manager and completely different from ItemPropEntityManager... ff
class LivingEntitiesManager
{
	Shader characterShader; // Should probably reuse another one but it is what it is
	std::vector<std::shared_ptr<IClientEntity>> characters;

	public:

		LivingEntitiesManager();
		~LivingEntitiesManager();

		void add(std::shared_ptr<IClientEntity> character);
		void draw(const glm::mat4 &projection, const glm::mat4 &view, const float deltatima);
};

#endif