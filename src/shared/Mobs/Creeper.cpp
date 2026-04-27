
#include "Creeper.hpp"
#include <cmath>

Creeper::Creeper(const glm::vec3 &position):	LivingEntity(position)
{
	type = CREEPER;

	this->entityWidth  = 0.6f;
	this->entityHeight = 1.7f;

	this->health = 20;
	this->damage = 0; // creepers don't melee; they explode
}

Creeper::Creeper(const glm::vec3 &position, float yaw, entityID ID): LivingEntity(position, yaw ,ID)
{
	type = CREEPER;

	this->entityWidth  = 0.6f;
	this->entityHeight = 1.7f;

	this->health = 20;
	this->damage = 0;
}

void Creeper::tickAI(const ICommonWorld &world, const std::vector<std::shared_ptr<LivingEntity>> &entities, int32_t tick)
{
	(void)world;
	(void)tick;

	LivingEntity *target = findNearestSurvivalPlayer(entities, MOB_FOLLOW_RADIUS, MOB_VERTICAL_TOLERANCE);

	isChasing = target != nullptr;
	if (target) {
		glm::dvec3 d = target->getPositionD() - this->position;
		float distH = static_cast<float>(std::sqrt(d.x * d.x + d.z * d.z));

		setYawTracked(static_cast<float>(glm::degrees(std::atan2(d.z, d.x))));

		if (distH < FUSE_RADIUS && std::abs(d.y) < MOB_VERTICAL_TOLERANCE) {
			// In fuse range: stand still and count up. Explode when the counter fills.
			aiWantsMove = false;
			aiMoveDir = glm::vec2(0.0f);
			isPrimed = true;
			fuseTicks++;
			if (fuseTicks >= FUSE_TICKS_MAX) {
				wantsExplode = true;
			}
		} else {
			// Walk toward target, fuse cools down when we lose range.
			glm::vec2 dirXZ(static_cast<float>(d.x), static_cast<float>(d.z));
			float len = glm::length(dirXZ);
			if (len > EPS) {
				aiMoveDir = dirXZ / len;
				aiWantsMove = true;
			} else {
				aiWantsMove = false;
			}
			if (fuseTicks > 0) fuseTicks--;
			if (fuseTicks == 0) isPrimed = false;
		}
		wanderTicksLeft = 0;
	} else {
		// No target -> wander. Fuse fully resets.
		if (fuseTicks > 0) fuseTicks--;
		if (fuseTicks == 0) isPrimed = false;
		wanderStep();
	}

	hasHorizontalInput = aiWantsMove;

	// Force a rotation-delta packet on any primed-state transition so clients
	// don't get stuck rendering the pulse after the fuse cools down. Server::
	// sendEntitiesPositionDeltas only transmits when one of position/rotation/
	// armSwing changed, and a standing-still creeper has none of the others.
	if (networkedPrimed != isPrimed)
		rotationUpdated = true;
	networkedPrimed = isPrimed;
}
