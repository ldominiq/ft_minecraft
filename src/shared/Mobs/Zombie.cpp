
#include "Zombie.hpp"
#include "PlayerMovement.hpp"
#include <cstdlib>
#include <cmath>

Zombie::Zombie(const glm::vec3 &position) : LivingEntity(position)
{
	type = ZOMBIE;

	this->entityWidth = 0.6f;
	this->entityHeight = 1.95f;

	this->health = 20;
	this->damage = 3;
}

Zombie::Zombie(const glm::vec3 &position, float yaw, entityID ID) : LivingEntity(position, yaw, ID)
{
	type = ZOMBIE;

	this->entityWidth = 0.6f;
	this->entityHeight = 1.95f;

	this->health = 20;
	this->damage = 3;
}

glm::vec3 Zombie::getDesiredMove()
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

void Zombie::calculateNewPosition(const ICommonWorld &world)
{
	jump = false;

	// Auto-jump: if we want to walk but a solid block sits directly in front at feet height,
	// try to hop over it. Simple single-block obstacle handling - no pathfinding.
	if (aiWantsMove && onGround) {
		glm::vec3 ahead = position + glm::vec3(aiMoveDir.x * 0.35f, 0.05f, aiMoveDir.y * 0.35f);
		AABB probe = constructAABB(ahead);
		if (aabbCollidesWithWorld(probe, world))
			jump = true;
	}

	doJump(world);
	glm::vec3 desiredMove = getDesiredMove();
	calculateNewXZPosition(world, desiredMove);
	calculateNewYPosition(world);
	jump = false;
}

void Zombie::tickAI(const ICommonWorld &world, const std::vector<std::shared_ptr<LivingEntity>> &entities, int32_t tick)
{
	(void)world;
	(void)tick;

	if (attackCooldownTicks > 0) attackCooldownTicks--;

	// Find nearest survival-mode player within follow radius (horizontal distance).
	LivingEntity* target = nullptr;
	float bestDistSq = FOLLOW_RADIUS * FOLLOW_RADIUS;
	for (auto &e : entities) {
		if (!e || e.get() == this) continue;
		if (e->getLivingEntityType() != PLAYER) continue;

		auto* pm = dynamic_cast<PlayerMovement*>(e.get());
		if (!pm) continue;
		if (pm->gamemode != GAMEMODES::SURVIVAL) continue;
		if (pm->health <= 0) continue;

		glm::vec3 d = pm->getPosition() - this->position;
		if (std::abs(d.y) > VERTICAL_TOLERANCE * 2.0f) continue;
		float dist2 = d.x * d.x + d.z * d.z;
		if (dist2 < bestDistSq) {
			bestDistSq = dist2;
			target = pm;
		}
	}

	auto setYaw = [&](float newYaw) {
		while (newYaw > 180.f)  newYaw -= 360.f;
		while (newYaw < -180.f) newYaw += 360.f;
		if (std::abs(newYaw - yaw) > 0.5f) rotationUpdated = true;
		yaw = newYaw;
	};

	isChasing = target != nullptr;
	if (target) {
		glm::vec3 d = target->getPosition() - this->position;
		float distH = std::sqrt(d.x * d.x + d.z * d.z);

		setYaw(glm::degrees(std::atan2(d.z, d.x)));

		if (distH < ATTACK_RADIUS && std::abs(d.y) < VERTICAL_TOLERANCE) {
			aiWantsMove = false;
			aiMoveDir = glm::vec2(0.0f);
			if (attackCooldownTicks == 0) {
				this->attack(*target);
				pendingArmSwing = true;
				attackCooldownTicks = ATTACK_COOLDOWN;
			}
		} else {
			glm::vec2 dirXZ(d.x, d.z);
			float len = glm::length(dirXZ);
			if (len > EPS) {
				aiMoveDir = dirXZ / len;
				aiWantsMove = true;
			} else {
				aiWantsMove = false;
			}
		}
		wanderTicksLeft = 0;
	} else {
		// No target -> wander. Pick a new direction every couple seconds.
		if (wanderTicksLeft <= 0) {
			int r = std::rand() % 4;
			if (r == 0) {
				aiWantsMove = false;
				aiMoveDir = glm::vec2(0.0f);
			} else {
				float wanderYawRad = glm::radians(static_cast<float>(std::rand() % 360));
				aiMoveDir = glm::vec2(std::cos(wanderYawRad), std::sin(wanderYawRad));
				aiWantsMove = true;
				setYaw(glm::degrees(wanderYawRad));
			}
			wanderTicksLeft = 40 + (std::rand() % 60);
		}
		wanderTicksLeft--;
	}

	hasHorizontalInput = aiWantsMove;
}
