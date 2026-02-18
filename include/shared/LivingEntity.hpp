
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
};

class LivingEntity : public Entity
{
	protected :
		float accumulatedFallDistance = 0.0f;

		bool jump = false;

		float movementSpeed = WALKING_SPEED; //deprecated?
		glm::vec3 Right = glm::vec3(0, 0, 0);

		virtual void doJump(const ICommonWorld &world);

		LivingEntityType type;

	public:
		int16_t health = 20;

		LivingEntity(const glm::vec3 &position);
		LivingEntity(const glm::vec3 &position, float yaw, entityID ID);
		virtual ~LivingEntity() = 0;

		glm::vec3 Front = glm::vec3(0, 0, 0);
		glm::vec3 WorldUp = glm::vec3(0, 1, 0);

		virtual void applyFallDamage();
		void calculateNewYPosition(const ICommonWorld &world) override;
		inline EEntityTypes getEntityType() const override { return EEntityTypes::LIVING_ENTITIES; }
		inline LivingEntityType getLivingEntityType() const { return type; }
		glm::vec3 getDesiredMove() override;
};

#endif
