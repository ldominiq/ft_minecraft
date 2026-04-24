
#include "LivingEntity.hpp"

//server
LivingEntity::LivingEntity(const glm::vec3 &position) : Entity(position) {}

//client
LivingEntity::LivingEntity(const glm::vec3 &position, float yaw, entityID ID): Entity(position, yaw ,ID) {}

LivingEntity::~LivingEntity() {}

void LivingEntity::doJump(const ICommonWorld &world)
{
	AABB boxFeetProbe = this->constructAABB(glm::vec3(this->position.x, this->position.y -EPS - 0.01f, this->position.z));
	onGround = this->aabbCollidesWithWorld(boxFeetProbe, world);

	if (this->jump && onGround)
		this->velocity.y = JUMP_VELOCITY;
}

void LivingEntity::attack(LivingEntity &victim)
{
	glm::vec3 knockbackDir{
		std::cos(glm::radians(yaw)),
		0.0f,
		std::sin(glm::radians(yaw))
	};

	constexpr float strength = 0.4f * 25;		//25 is completely arbitrary testing value
	constexpr float verticalBoost = 0.1 * 5;	//same with 5

	victim.health -= damage;
	victim.applyImpulse(knockbackDir * strength + glm::vec3(0.0f, verticalBoost, 0.0f));
}

void LivingEntity::onDeath()
{
	// TODO : drop something?
}

void LivingEntity::applyFallDamage()
{
	const uint16_t FALL_DAMAGE_MULTIPLIER = 1; //temporally here just to give the idea in case it ends up being used
	float fallDamage = std::max(0, (int)std::ceil((accumulatedFallDistance - SAFE_FALL_DISTANCE) * FALL_DAMAGE_MULTIPLIER));
	if (fallDamage >= health)
		health = 0;
	else
		health -= fallDamage;
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

	if (this->position.y < 0)
	{
		auto now = std::chrono::steady_clock::now();

		if (now - lastVoidDamageTime >= std::chrono::seconds(1))
		{
			this->health -= 4;
			lastVoidDamageTime = now;
		}
	}
}

glm::vec3 LivingEntity::getDesiredMove()
{
	// TODO . just like DoJump....

	float effectMultiplier = 1.0f; 
	float slipperiness = SM_DEFAULT; 
	slipperiness_prev = slipperiness;

	float movementMultiplier = MM_WALKING;

	glm::vec2 inputWorld; //temporally here while mob still has no movement
    inputWorld.x = 0;
    inputWorld.y = 0;


	glm::vec2 prevV(this->velocity.x, this->velocity.z);
    glm::vec2 momentum = prevV * (slipperiness_prev * 0.91f);

	float accelGround = 0.1f * movementMultiplier * effectMultiplier * std::pow(0.6f / slipperiness, 3.0f);
	float accelAir    = 0.02f * movementMultiplier;
	float accel = onGround ? accelGround : accelAir;

	float isMoving = 0; //temporally here while mob still has no movement
	glm::vec2 accelVec = inputWorld * accel * isMoving;

    glm::vec2 newV = momentum + accelVec;

	this->velocity.x = newV.x;
	this->velocity.z = newV.y;

	slipperiness_prev = slipperiness;

	if (std::abs(this->velocity.x) < EPS) this->velocity.x = 0;
	if (std::abs(this->velocity.z) < EPS) this->velocity.z = 0;

	return glm::vec3(this->velocity.x,0,this->velocity.z);
}
