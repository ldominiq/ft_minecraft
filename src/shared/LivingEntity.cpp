
#include "LivingEntity.hpp"

//server
LivingEntity::LivingEntity(glm::vec3 &position) : Entity(position) {}

//client
LivingEntity::LivingEntity(glm::vec3 &position, float yaw, entityID ID): Entity(position, yaw ,ID) {}

LivingEntity::~LivingEntity() {}

void LivingEntity::doJump(const ICommonWorld &world)
{
	AABB boxFeetProbe = this->constructAABB(glm::vec3(this->position.x, this->position.y -EPS - 0.01f, this->position.z));
	bool onGround = this->aabbCollidesWithWorld(boxFeetProbe, world);

	if (this->jump && onGround)
		this->velocity.y = JUMP_VELOCITY;
}

glm::vec3 LivingEntity::getDesiredMove()
{
	// TODO . just like DoJump....
	return glm::vec3(0,0,0);
}