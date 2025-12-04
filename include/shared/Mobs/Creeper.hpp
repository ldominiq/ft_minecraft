#ifndef CREEPER_HPP
#define CREEPER_HPP

#include "LivingEntity.hpp"

struct Creeper : public virtual LivingEntity {

	public:

		Creeper(const glm::vec3 &position);
		Creeper(const glm::vec3 &position, float yaw, entityID ID);
		virtual ~Creeper() = default;

		glm::vec3 getDesiredMove() override;
		void calculateNewPosition(const ICommonWorld &world) override;
};

#endif