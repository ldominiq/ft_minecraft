
#include "LivingEntity.hpp"
#include "PlayerMovement.hpp"
#include <random>

// Shared RNG for all mob AI wander/spawn decisions. Seeded once from a
// non-deterministic source so behavior differs across server runs.
static std::mt19937 &mobRng()
{
	static std::mt19937 rng(std::random_device{}());
	return rng;
}

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
	float slipperiness = SM_DEFAULT;
	slipperiness_prev = slipperiness;

	float movementMultiplier = isChasing ? MM_WALKING : MM_SNEAKING;

	glm::vec2 inputWorld = aiWantsMove ? aiMoveDir : glm::vec2(0.0f);
	float isMoving = aiWantsMove ? 1.0f : 0.0f;

	glm::vec2 prevV(this->velocity.x, this->velocity.z);
	glm::vec2 momentum = prevV * (slipperiness_prev * 0.91f);

	float accelGround = 0.1f * movementMultiplier * std::pow(0.6f / slipperiness, 3.0f);
	float accelAir    = 0.02f * movementMultiplier;
	float accel = onGround ? accelGround : accelAir;

	glm::vec2 accelVec = inputWorld * accel * isMoving;
	glm::vec2 newV = momentum + accelVec;

	this->velocity.x = newV.x;
	this->velocity.z = newV.y;

	if (std::abs(this->velocity.x) < EPS) this->velocity.x = 0;
	if (std::abs(this->velocity.z) < EPS) this->velocity.z = 0;

	return glm::vec3(this->velocity.x, 0, this->velocity.z);
}

void LivingEntity::mobAutoJump(const ICommonWorld &world)
{
	if (aiWantsMove && onGround) {
		glm::vec3 ahead = position + glm::vec3(aiMoveDir.x * 0.35f, 0.05f, aiMoveDir.y * 0.35f);
		AABB probe = constructAABB(ahead);
		if (aabbCollidesWithWorld(probe, world))
			jump = true;
	}
}

void LivingEntity::calculateNewPosition(const ICommonWorld &world)
{
	jump = false;
	mobAutoJump(world);
	doJump(world);
	glm::vec3 desiredMove = getDesiredMove();
	calculateNewXZPosition(world, desiredMove);
	calculateNewYPosition(world);
	jump = false;
}

LivingEntity *LivingEntity::findNearestSurvivalPlayer(
	const std::vector<std::shared_ptr<LivingEntity>> &entities,
	float followRadius, float verticalTolerance)
{
	LivingEntity *target = nullptr;
	float bestDistSq = followRadius * followRadius;
	for (auto &e : entities) {
		if (!e || e.get() == this) continue;
		if (e->getLivingEntityType() != PLAYER) continue;

		auto *pm = dynamic_cast<PlayerMovement *>(e.get());
		if (!pm) continue;
		if (pm->gamemode != GAMEMODES::SURVIVAL) continue;
		if (pm->health <= 0) continue;

		glm::vec3 d = pm->getPosition() - this->position;
		if (std::abs(d.y) > verticalTolerance * 2.0f) continue;
		float dist2 = d.x * d.x + d.z * d.z;
		if (dist2 < bestDistSq) {
			bestDistSq = dist2;
			target = pm;
		}
	}
	return target;
}

void LivingEntity::setYawTracked(float newYaw)
{
	while (newYaw > 180.f)  newYaw -= 360.f;
	while (newYaw < -180.f) newYaw += 360.f;
	if (std::abs(newYaw - yaw) > 0.5f) rotationUpdated = true;
	yaw = newYaw;
}

void LivingEntity::wanderStep()
{
	if (wanderTicksLeft <= 0) {
		auto &rng = mobRng();
		int r = std::uniform_int_distribution<int>(0, 3)(rng);
		if (r == 0) {
			aiWantsMove = false;
			aiMoveDir = glm::vec2(0.0f);
		} else {
			float wanderYawRad = glm::radians(static_cast<float>(std::uniform_int_distribution<int>(0, 359)(rng)));
			aiMoveDir = glm::vec2(std::cos(wanderYawRad), std::sin(wanderYawRad));
			aiWantsMove = true;
			setYawTracked(glm::degrees(wanderYawRad));
		}
		wanderTicksLeft = 40 + std::uniform_int_distribution<int>(0, 59)(rng);
	}
	wanderTicksLeft--;
}
