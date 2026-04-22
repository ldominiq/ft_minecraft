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
		void tickAI(const ICommonWorld &world, const std::vector<std::shared_ptr<LivingEntity>> &entities, int32_t tick) override;

		// Set to true when the fuse reaches the trigger threshold.
		// World::updateEntitiesPosition consumes this flag post-tickAI.
		bool  wantsExplode = false;
		// True while the fuse is ticking up (target in range). Networked to clients
		// via NetEntityMove::positionFlags so the model can flash/swell.
		bool  isPrimed = false;
		float explodeRadius = 3.5f;
		float explodeDamage = 20.0f;

		// ticks into the fuse [0, FUSE_TICKS_MAX]
		int32_t fuseTicks = 0;

		static constexpr float FOLLOW_RADIUS      = 16.0f;
		static constexpr float FUSE_RADIUS        = 2.5f;
		static constexpr float VERTICAL_TOLERANCE = 3.0f;
		static constexpr int32_t FUSE_TICKS_MAX   = 30; // ~1.5s at 20 TPS

	private:
		glm::vec2 aiMoveDir   = glm::vec2(0.0f);
		bool      aiWantsMove = false;
		bool      isChasing   = false;
		int32_t   wanderTicksLeft = 0;
};

#endif
