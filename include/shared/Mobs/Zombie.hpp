#ifndef ZOMBIE_HPP
#define ZOMBIE_HPP

#include "LivingEntity.hpp"

struct Zombie : public virtual LivingEntity {

	public:

		Zombie(const glm::vec3 &position);
		Zombie(const glm::vec3 &position, float yaw, entityID ID);
		virtual ~Zombie() = default;

		glm::vec3 getDesiredMove() override;
		void calculateNewPosition(const ICommonWorld &world) override;
		void tickAI(const ICommonWorld &world, const std::vector<std::shared_ptr<LivingEntity>> &entities, int32_t tick) override;

	private:
		// Horizontal unit direction we want to walk this tick (server-authoritative).
		glm::vec2 aiMoveDir = glm::vec2(0.0f);
		bool      aiWantsMove = false;
		bool      isChasing = false;

		int32_t attackCooldownTicks = 0;
		int32_t wanderTicksLeft = 0;
		int32_t stuckTicks = 0;

		// Detection radius (blocks). Beyond this, wander only.
		static constexpr float FOLLOW_RADIUS = 16.0f;
		static constexpr float ATTACK_RADIUS = 1.8f;
		static constexpr float VERTICAL_TOLERANCE = 3.0f;
		static constexpr int32_t ATTACK_COOLDOWN = 20; // ticks (~1s at 20 TPS)
};

#endif
