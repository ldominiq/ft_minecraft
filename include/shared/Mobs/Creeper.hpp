#ifndef CREEPER_HPP
#define CREEPER_HPP

#include "LivingEntity.hpp"

struct Creeper : public virtual LivingEntity {
	float wanderCooldown = 0.0f;
	float wanderDuration = 0.0f;
	glm::vec3 wanderDirection = glm::vec3(0.0f);

	const float DELTA_TIME = 1.0f / 20.0f; // Server runs at 20 TPS

	public:

		Creeper(const glm::vec3 &position);
		Creeper(const glm::vec3 &position, float yaw, entityID ID);
		virtual ~Creeper() = default;

		glm::vec3 getDesiredMove(const ICommonWorld &world, const std::vector<std::shared_ptr<Entity>> &players);
		void calculateNewPosition(const ICommonWorld &world, const std::vector<std::shared_ptr<Entity>> &players) override;
		void checkObstacleAndJump(const ICommonWorld &world, const glm::vec3 &direction);

		glm::vec3 getWanderMove(const ICommonWorld &world);
};

#endif