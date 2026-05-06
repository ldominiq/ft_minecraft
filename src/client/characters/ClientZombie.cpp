
#include "ClientZombie.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ClientZombie::ClientZombie(const glm::vec3 &position, float yaw, entityID ID) : Zombie(position, yaw, ID), IClientEntity(position, yaw, ID), Character(), LivingEntity(position, yaw, ID)
{
	setPartsDimensions();
	createCharacterAt(position, entityWidth, entityHeight);

	// Lock arms forward at spawn so idle zombies also hold the pose.
	float pivotY = torsoScaleY / 2.0f;
	const float forwardAngle = static_cast<float>(M_PI) * 0.5f;
	rotateBodyPartAxis(characterBodyParts.leftArm, pivotY, forwardAngle, glm::vec3(0, 0, 1), 1.0f);
	rotateBodyPartAxis(characterBodyParts.rightArm, pivotY, forwardAngle, glm::vec3(0, 0, 1), 1.0f);
}

// Zombie-style: arms extend straight forward, legs still swing while walking.
void ClientZombie::walkAnimation(float deltaTime)
{
	characterBodyParts.onWalkAnimation = true;
	float oldPhase = characterBodyParts.walkPhase;
	characterBodyParts.walkPhase += deltaTime * characterBodyParts.walkingAnimationSpeed;

	if (std::floor(oldPhase) != std::floor(characterBodyParts.walkPhase)) {
		characterBodyParts.walkPhase = 0.0f;
		characterBodyParts.onWalkAnimation = false;
	}
	else if (oldPhase < 0.5f && characterBodyParts.walkPhase >= 0.5f) {
		characterBodyParts.walkPhase = 0.5f;
		characterBodyParts.onWalkAnimation = false;
	}

	float angle = std::sin(characterBodyParts.walkPhase * 2.0f * M_PI);
	float pivotY = torsoScaleY / 2.0f;

	// Arms: locked forward ~90°, with a small sway so they don't look perfectly rigid.
	const float forwardAngle = static_cast<float>(M_PI) * 0.5f;
	float armSway = angle * 0.15f;

	rotateBodyPartAxis(characterBodyParts.leftArm, pivotY, forwardAngle + armSway, glm::vec3(0, 0, 1), 1.0f);
	if (!characterBodyParts.onArmSwingAnimation)
		rotateBodyPartAxis(characterBodyParts.rightArm, pivotY, forwardAngle - armSway, glm::vec3(0, 0, 1), 1.0f);

	// Legs swing normally.
	float legPivotY = -torsoScaleY / 2.0f;
	rotateBodyPart(characterBodyParts.leftLeg, legPivotY, -angle * 0.8f);
	rotateBodyPart(characterBodyParts.rightLeg, legPivotY, angle * 0.8f);
}
