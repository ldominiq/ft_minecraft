
#include "Zombie.hpp"
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

void Zombie::tickAI(const ICommonWorld &world, const std::vector<std::shared_ptr<LivingEntity>> &entities, int32_t tick)
{
	(void)world;
	(void)tick;

	if (attackCooldownTicks > 0) attackCooldownTicks--;

	LivingEntity *target = findNearestSurvivalPlayer(entities, MOB_FOLLOW_RADIUS, MOB_VERTICAL_TOLERANCE);

	isChasing = target != nullptr;
	if (target) {
		glm::vec3 d = target->getPosition() - this->position;
		float distH = std::sqrt(d.x * d.x + d.z * d.z);

		setYawTracked(glm::degrees(std::atan2(d.z, d.x)));

		if (distH < ATTACK_RADIUS && std::abs(d.y) < MOB_VERTICAL_TOLERANCE) {
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
		wanderStep();
	}

	hasHorizontalInput = aiWantsMove;
}
