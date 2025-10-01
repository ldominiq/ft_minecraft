
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

class LivingEntity : public Entity
{
	protected :
		bool jump = false;

		float movementSpeed = WALKING_SPEED; //deprecated?
		glm::vec3 velocity = glm::vec3(0, 0, 0);	//maybe only needed in player? Or should mobs also have momentum

		glm::vec3 Right = glm::vec3(0, 0, 0);
		glm::vec3 WorldUp = glm::vec3(0, 1, 0);

		virtual glm::vec3 getDesiredMove() = 0;
		virtual void doJump(const ICommonWorld &world) = 0;

	public:
		glm::vec3 Front = glm::vec3(0, 0, 0);
		glm::vec3 Up = glm::vec3(0, 0, 0);

		LivingEntity(glm::vec3 position);
		virtual ~LivingEntity() = 0;
};

#endif
