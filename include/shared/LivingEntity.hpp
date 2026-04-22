
#ifndef LIVING_ENTITY_HPP
#define LIVING_ENTITY_HPP

#include "Entity.hpp"

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
	protected :
		bool jump = false;
		float SAFE_FALL_DISTANCE = 3.0f;

		float movementSpeed = WALKING_SPEED; //deprecated?
		glm::vec3 Right = glm::vec3(0, 0, 0);

		virtual void doJump(const ICommonWorld &world);

		LivingEntityType type;

		//used for players
		const float forehead = 0.3f;
		float eyesheight = 0.0f;

	public:
		float health = 20;
		float damage = 5;
		float accumulatedFallDistance = 0.0f;

		LivingEntity(const glm::vec3 &position);
		LivingEntity(const glm::vec3 &position, float yaw, entityID ID);
		virtual ~LivingEntity() = 0;

		glm::vec3 Front = glm::vec3(0, 0, 0);
		glm::vec3 WorldUp = glm::vec3(0, 1, 0);

		virtual void attack(LivingEntity &victim);
		virtual void onDeath();
		virtual void applyFallDamage();
		virtual void tickAI(const ICommonWorld &world, const std::vector<std::shared_ptr<LivingEntity>> &entities, int32_t tick) { (void)world; (void)entities; (void)tick; }
		void calculateNewYPosition(const ICommonWorld &world) override;
		inline EEntityTypes getEntityType() const override { return EEntityTypes::LIVING_ENTITIES; }
		inline LivingEntityType getLivingEntityType() const { return type; }
		inline float getEyesHeight() const { return eyesheight; }
		glm::vec3 getDesiredMove() override;
		float getAccumulatedFallDistance() const { return accumulatedFallDistance; }
};

#endif
