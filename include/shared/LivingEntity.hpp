
#ifndef LIVING_ENTITY_HPP
#define LIVING_ENTITY_HPP

#include "Entity.hpp"
#include <memory>
#include <vector>

//MOVEMENT MULTIPLIERS
#define MM_WALKING		1.0f
#define MM_SPRINTING	1.3f
#define MM_SNEAKING		0.3f
#define MM_STOPPING		0.0f

#define MM_DEFAULT		0.98f
#define MM_STRAFE		1.0f
#define MM_SNEAK_STRAFE (0.98f * 1.41421356237f)

#define WALKING_SPEED	4.317f

#define JUMP_VELOCITY	0.42f

enum LivingEntityType : uint16_t
{
	PLAYER = 0,
	CREEPER = 4,
	ZOMBIE = 5,
};

class LivingEntity : public Entity
{
	std::chrono::steady_clock::time_point lastVoidDamageTime{};

	protected :
		std::string name = "";

		bool jump = false;
		float SAFE_FALL_DISTANCE = 3.0f;

		float movementSpeed = WALKING_SPEED; //deprecated?
		glm::vec3 Right = glm::vec3(0, 0, 0);

		virtual void doJump(const ICommonWorld &world);

		LivingEntityType type;

		//used for players
		const float forehead = 0.3f;
		float eyesheight = 0.0f;

		// Shared mob-AI state. Populated by subclass tickAI overrides via the
		// helpers below; consumed by the shared getDesiredMove/calculateNewPosition.
		glm::vec2 aiMoveDir = glm::vec2(0.0f);
		bool      aiWantsMove = false;
		bool      isChasing = false;
		int32_t   wanderTicksLeft = 0;

		// Shared mob-AI constants (override in subclass if they need different radii).
		static constexpr float MOB_FOLLOW_RADIUS = 16.0f;
		static constexpr float MOB_VERTICAL_TOLERANCE = 3.0f;

		LivingEntity *findNearestSurvivalPlayer(const std::vector<std::shared_ptr<LivingEntity>> &entities,
		                                        float followRadius, float verticalTolerance);
		void setYawTracked(float newYaw);
		void wanderStep();
		void mobAutoJump(const ICommonWorld &world);

	public:
		float health = 20;
		float damage = 5;
		float accumulatedFallDistance = 0.0f;

		// Death-animation bookkeeping (server-authoritative).
		// When health hits 0, sendDeaths broadcasts the death packet once and sets
		// pendingDeathRemovalTicks > 0 so the entity lingers for the fall-over animation.
		// `diedByExplosion` skips the delay (the body physically vanishes in the blast).
		bool deathBroadcast = false;
		bool diedByExplosion = false;
		int32_t pendingDeathRemovalTicks = 0;

		// Generic "fused"/priming flag sent to clients in NetEntityMove::positionFlags bit 0x08.
		// Currently only creepers set this.
		bool networkedPrimed = false;

		LivingEntity(const glm::vec3 &position);
		LivingEntity(const glm::vec3 &position, float yaw, entityID ID);
		virtual ~LivingEntity() = 0;

		glm::vec3 Front = glm::vec3(0, 0, 0);
		glm::vec3 WorldUp = glm::vec3(0, 1, 0);

		virtual void attack(LivingEntity &victim);
		virtual void onDeath(const ICommonWorld &world);
		virtual void applyFallDamage();
		virtual void tickAI(const ICommonWorld &world, const std::vector<std::shared_ptr<LivingEntity>> &entities, int32_t tick) { (void)world; (void)entities; (void)tick; }
		void calculateNewYPosition(const ICommonWorld &world) override;
		void calculateNewPosition(const ICommonWorld &world) override;
		inline EEntityTypes getEntityType() const override { return EEntityTypes::LIVING_ENTITIES; }
		inline LivingEntityType getLivingEntityType() const { return type; }
		inline float getEyesHeight() const { return eyesheight; }
		glm::vec3 getDesiredMove() override;
		float getAccumulatedFallDistance() const { return accumulatedFallDistance; }

		void setName(const std::string& name) { this->name = name; }
		const std::string& getName() const { return name; }
};

#endif
