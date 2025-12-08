#ifndef CREEPER_HPP
#define CREEPER_HPP

#include "LivingEntity.hpp"

struct Creeper : public virtual LivingEntity {

	public:

		Creeper(const glm::vec3 &position);
		Creeper(const glm::vec3 &position, float yaw, entityID ID);
		virtual ~Creeper() = default;

		glm::vec3 getDesiredMove(const std::vector<std::shared_ptr<Entity>> &players);
		void calculateNewPosition(const ICommonWorld &world, const std::vector<std::shared_ptr<Entity>> &players) override;
};

#endif