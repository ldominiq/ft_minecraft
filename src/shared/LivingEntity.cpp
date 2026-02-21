
#include "LivingEntity.hpp"

//server
LivingEntity::LivingEntity(const glm::vec3 &position) : Entity(position) {}

//client
LivingEntity::LivingEntity(const glm::vec3 &position, float yaw, entityID ID): Entity(position, yaw ,ID) {}

LivingEntity::~LivingEntity() {}

void LivingEntity::doJump(const ICommonWorld &world)
{
	AABB boxFeetProbe = this->constructAABB(glm::vec3(this->position.x, this->position.y -EPS - 0.01f, this->position.z));
	bool onGround = this->aabbCollidesWithWorld(boxFeetProbe, world);

	if (this->jump && onGround)
		this->velocity.y = JUMP_VELOCITY;
}

void LivingEntity::onDeath()
{
	// TODO : drop something?
}

void LivingEntity::applyFallDamage()
{
	const uint16_t FALL_DAMAGE_MULTIPLIER = 1; //temporally here just to give the idea in case it ends up being used
	float fallDamage = std::max(0, (int)std::ceil((accumulatedFallDistance - 3.0f) * FALL_DAMAGE_MULTIPLIER));
	auto prevH = health;
	health -= fallDamage;
	if (prevH != health)
		std::cout << "HEALTH DROPPED BY : " << fallDamage << "\n";
}

void LivingEntity::calculateNewYPosition(const ICommonWorld &world)
{
	Entity::calculateNewYPosition(world);

	// Each tick
	if (velocity.y < 0 && !onGround)
		accumulatedFallDistance += -velocity.y;
	else if (onGround)
	{
		applyFallDamage();
		accumulatedFallDistance = 0.0f;
	}
}

glm::vec3 LivingEntity::getDesiredMove()
{
	// TODO . just like DoJump....
	return glm::vec3(0,0,0);
}