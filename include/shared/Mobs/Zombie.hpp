#ifndef ZOMBIE_HPP
#define ZOMBIE_HPP

#include "LivingEntity.hpp"

struct Zombie : public virtual LivingEntity {

	public:

		Zombie(const glm::vec3 &position);
		Zombie(const glm::vec3 &position, float yaw, entityID ID);
		virtual ~Zombie() = default;

		void tickAI(const ICommonWorld &world, const std::vector<std::shared_ptr<LivingEntity>> &entities, int32_t tick) override;

	private:
		int32_t attackCooldownTicks = 0;

		static constexpr float ATTACK_RADIUS = 1.8f;
		static constexpr int32_t ATTACK_COOLDOWN = 20; // ticks (~1s at 20 TPS)
};

#endif
