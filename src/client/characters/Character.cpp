#include "Character.hpp"
#include "SkinBox.hpp"
#include <cmath>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Character::Character()
{
}

void Character::setPartsDimensions()
{
    torsoScaleX = 4.0f;   // depth
    torsoScaleY = 12.0f;
    torsoScaleZ = 8.0f;   // width

    headScaleX = 7.0f;
    headScaleY = 7.0f;
    headScaleZ = 7.0f;
    headTransY = (torsoScaleY * 0.5f + headScaleY * 0.5f) / headScaleY;

    // Split limbs: 6 + 6
    armScaleX = 4.0f;
    armScaleY = 6.0f;
    armScaleZ = 4.0f;

    armTransZ = (torsoScaleZ * 0.5f + armScaleZ * 0.5f) / armScaleZ;

    armTransY = ((torsoScaleY - armScaleY) * 0.5f) / armScaleY;

    legScaleX = 4.0f;
    legScaleY = 6.0f;
    legScaleZ = 4.0f;
    legTransZ = (legScaleZ * 0.5f) / legScaleZ; // keep legs just inside the torso edges
    legTransY = -((torsoScaleY * 0.5f + legScaleY * 0.5f) / legScaleY);

    // Total height = 7 + 12 + 12 = 31
    characterYScaleNorm = 1.0f / (torsoScaleY + headScaleY + legScaleY * 2.0f);

    // Keep X/Z isotropic so cubes stay cubes in horizontal plane
    characterXScaleNorm = 1.0f / headScaleX; // 1/7
    characterZScaleNorm = 1.0f / headScaleZ; // 1/7

    feetPositionY = torsoScaleY * 0.5f + legScaleY * 2.0f;
    YPositionOffset = glm::vec3(0, feetPositionY * characterYScaleNorm, 0);
}

void Character::createCharacterAt(const glm::vec3 &pos, float width, float height)
{
    Space character;

    float targetHorizontal = height * (headScaleY * characterYScaleNorm);
    float horizontal = std::min(width, targetHorizontal);

    character.scale = glm::scale(glm::mat4(1.0f), glm::vec3(
        horizontal * characterXScaleNorm,
        height * characterYScaleNorm,
        horizontal * characterZScaleNorm));
    YPositionOffset = glm::vec3(0, feetPositionY * characterYScaleNorm * height, 0);
    character.translation = glm::translate(glm::mat4(1.0f), pos + YPositionOffset);

    // Skin UV boxes on the classic 64x32 Steve layout.
    // Arms/legs on the skin are h=12, but the rig splits each into upper + lower
    // (each half-height), so we use vertical sub-slices.
    constexpr int SW = 64, SH = 32;
    auto headUVs       = boxUVs(0,  0, 8, 8, 8, SW, SH);
    auto torsoUVs      = boxUVs(16, 16, 8, 12, 4, SW, SH);
    auto armUpperUVs   = boxUVsSlice(40, 16, 4, 12, 4, SW, SH, 0.0f, 0.5f);
    auto armLowerUVs   = boxUVsSlice(40, 16, 4, 12, 4, SW, SH, 0.5f, 1.0f);
    auto legUpperUVs   = boxUVsSlice(0,  16, 4, 12, 4, SW, SH, 0.0f, 0.5f);
    auto legLowerUVs   = boxUVsSlice(0,  16, 4, 12, 4, SW, SH, 0.5f, 1.0f);

    // Torso
    auto torso = std::make_shared<Shape>(glm::vec3(1,0,0));
    torso->scale = glm::scale(glm::mat4(1.0f), glm::vec3(torsoScaleX, torsoScaleY, torsoScaleZ));
    torso->setSkinBox(torsoUVs);
    character.addChild(torso);

    // Head
    auto head = std::make_shared<Shape>(glm::vec3(0,1,0));
    head->scale = glm::scale(glm::mat4(1.0f), glm::vec3(headScaleX, headScaleY, headScaleZ));
    head->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, headTransY, 0));
    head->setSkinBox(headUVs);
    character.addChild(head);

    // Arms (upper)
    auto rightArm = std::make_shared<Shape>(glm::vec3(0,0,1));
    rightArm->scale = glm::scale(glm::mat4(1.0f), glm::vec3(armScaleX, armScaleY, armScaleZ));
    rightArm->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, armTransY, armTransZ));
    rightArm->setSkinBox(armUpperUVs);
    character.addChild(rightArm);

    auto leftArm = std::make_shared<Shape>(glm::vec3(0,0,1));
    leftArm->scale = glm::scale(glm::mat4(1.0f), glm::vec3(armScaleX, armScaleY, armScaleZ));
    leftArm->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, armTransY, -armTransZ));
    leftArm->setSkinBox(armUpperUVs);
    character.addChild(leftArm);

    // Forearms (lower arm, hanging below upper arm)
    auto rightForearm = std::make_shared<Shape>(glm::vec3(0.5f,0,1));
    rightForearm->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, 0));
    rightForearm->setSkinBox(armLowerUVs);
    rightArm->addChild(rightForearm);

    auto leftForearm = std::make_shared<Shape>(glm::vec3(0.5f,0,1));
    leftForearm->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, 0));
    leftForearm->setSkinBox(armLowerUVs);
    leftArm->addChild(leftForearm);

    // Legs (upper)
    auto rightLeg = std::make_shared<Shape>(glm::vec3(1,1,0));
    rightLeg->scale = glm::scale(glm::mat4(1.0f), glm::vec3(legScaleX, legScaleY, legScaleZ));
    rightLeg->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, legTransY, legTransZ));
    rightLeg->setSkinBox(legUpperUVs);
    character.addChild(rightLeg);

    auto leftLeg = std::make_shared<Shape>(glm::vec3(1,1,0));
    leftLeg->scale = glm::scale(glm::mat4(1.0f), glm::vec3(legScaleX, legScaleY, legScaleZ));
    leftLeg->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, legTransY, -legTransZ));
    leftLeg->setSkinBox(legUpperUVs);
    character.addChild(leftLeg);

    // Calves
    auto rightCalf = std::make_shared<Shape>(glm::vec3(1,1,0.5f));
    rightCalf->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, 0));
    rightCalf->setSkinBox(legLowerUVs);
    rightLeg->addChild(rightCalf);

    auto leftCalf = std::make_shared<Shape>(glm::vec3(1,1,0.5f));
    leftCalf->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, 0));
    leftCalf->setSkinBox(legLowerUVs);
    leftLeg->addChild(leftCalf);

    characterBodyParts.character = character;
    characterBodyParts.head = head;
    characterBodyParts.torso = torso;
    characterBodyParts.rightArm = rightArm;
    characterBodyParts.leftArm = leftArm;
    characterBodyParts.rightForearm = rightForearm;
    characterBodyParts.leftForearm = leftForearm;
    characterBodyParts.rightLeg = rightLeg;
    characterBodyParts.leftLeg = leftLeg;
    characterBodyParts.rightCalf = rightCalf;
    characterBodyParts.leftCalf = leftCalf;
}

void Character::rotateBodyPart(const std::shared_ptr<Shape>& bodyPart, float pivot, float angle) const
{
	rotateBodyPartAxis(bodyPart, pivot, angle, glm::vec3(0,0,1), 0.8f);
}

void Character::rotateBodyPartAxis(const std::shared_ptr<Shape>& bodyPart, float pivot, float angle, const glm::vec3 &axis, float multiplier) const
{
	bodyPart->rotation =
		glm::translate(glm::mat4(1.0f), glm::vec3(0, pivot, 0)) *
		extractScaleInverse(characterBodyParts.character.scale) *
		glm::rotate(glm::mat4(1.0f), angle * multiplier, axis) *
		characterBodyParts.character.scale *
		glm::translate(glm::mat4(1.0f), glm::vec3(0, -pivot, 0));
}

void Character::walkAnimation(float deltaTime)
{
	// Skip the walking arm swing on the right arm while a punch/swing animation is playing,
	// so the two don't fight each other.
	characterBodyParts.onWalkAnimation = true;
	float walkAmplitude = 1.0f;
	float oldPhase = characterBodyParts.walkPhase;

	// Advance phase
	characterBodyParts.walkPhase += deltaTime * characterBodyParts.walkingAnimationSpeed;

	// --- Detect full cycle completion ---
	if (floor(oldPhase) != floor(characterBodyParts.walkPhase))
	{
		// Completed a full animation cycle
		characterBodyParts.walkPhase = 0.0f;
		characterBodyParts.onWalkAnimation = false;
	}
	else if (oldPhase < 0.5f && characterBodyParts.walkPhase >= 0.5f)
	{
		characterBodyParts.walkPhase = 0.5f;
		characterBodyParts.onWalkAnimation = false;
	}

	// Angle for limbs
	float angle = sin(characterBodyParts.walkPhase * 2.0f * M_PI) * walkAmplitude;

	// ---- Arms ----
	float pivotY = torsoScaleY / 2.0f;

	rotateBodyPart(characterBodyParts.leftArm, pivotY, angle);
	if (!characterBodyParts.onArmSwingAnimation)
		rotateBodyPart(characterBodyParts.rightArm, pivotY, -angle);

	if (angle >= 0)
		rotateBodyPart(characterBodyParts.leftForearm, -armScaleY * 0.5f, angle * 1.2f);

	if (angle <= 0 && !characterBodyParts.onArmSwingAnimation)
		rotateBodyPart(characterBodyParts.rightForearm, -armScaleY * 0.5f, angle * -1.2f);

	// ---- Legs ----
	float legPivotY = -torsoScaleY / 2.0f;

	rotateBodyPart(characterBodyParts.leftLeg, legPivotY, -angle * 0.8f);
	rotateBodyPart(characterBodyParts.rightLeg, legPivotY, angle * 0.8f);
}

void Character::applyHeadPitch(float pitchDegrees)
{
	// Head neck pivot is at its bottom (Y = -headScaleY/2 in head-local space).
	// Positive pitch (looking up) tilts the head back around the side (Z) axis.
	float angle = glm::radians(pitchDegrees);
	rotateBodyPartAxis(characterBodyParts.head, headScaleY * 0.5f, angle, glm::vec3(0,0,1), 1.0f);
}

void Character::triggerArmSwing()
{
	characterBodyParts.onArmSwingAnimation = true;
	characterBodyParts.armSwingPhase = 0.0f;
}

void Character::swingArmAnimation(float deltaTime)
{
	if (!characterBodyParts.onArmSwingAnimation)
		return;

	characterBodyParts.armSwingPhase += deltaTime / armSwingDuration;
	if (characterBodyParts.armSwingPhase >= 1.0f)
	{
		characterBodyParts.onArmSwingAnimation = false;
		characterBodyParts.armSwingPhase = 0.0f;
		// Reset the right arm/forearm to rest — walking animation will take over if needed.
		characterBodyParts.rightArm->rotation = glm::mat4(1.0f);
		characterBodyParts.rightForearm->rotation = glm::mat4(1.0f);
		return;
	}

	// A single forward-down-backward punch. Peaks around the middle of the swing.
	float t = characterBodyParts.armSwingPhase;
	float curve = sin(t * M_PI); // 0 → 1 → 0

	// Arm rotates forward (bringing the forearm down in front of the body) and the
	// forearm bends a bit extra — mimics a simple overhead-ish punch/break motion.
	float shoulderAngle = curve * 1.6f;          // swing forward/down
	float elbowAngle    = curve * 0.6f;          // slight forearm bend

	float pivotY = +armScaleY * 0.5f;
	rotateBodyPartAxis(characterBodyParts.rightArm, pivotY, shoulderAngle, glm::vec3(0,0,1), 1.0f);
	rotateBodyPartAxis(characterBodyParts.rightForearm, -1.5f, elbowAngle, glm::vec3(0,0,1), 1.0f);
}

void Character::triggerDeath()
{
    if (characterBodyParts.dying)
        return;
    characterBodyParts.dying = true;
    characterBodyParts.dyingDone = false;
    characterBodyParts.dyingPhase = 0.0f;
}

void Character::deathAnimation(float deltaTime)
{
    constexpr float DEATH_DURATION = 1.0f;
    if (characterBodyParts.dyingDone)
        return;
    characterBodyParts.dyingPhase += deltaTime / DEATH_DURATION;
    if (characterBodyParts.dyingPhase >= 1.0f)
    {
        characterBodyParts.dyingPhase = 1.0f;
        characterBodyParts.dyingDone = true;
    }
    float angle = static_cast<float>(M_PI) * 0.5f * characterBodyParts.dyingPhase;
    // Root Space transform is translation * rotation * scale, so rotation pivots at
    // the character's local origin (torso center). We want the pivot at the feet so
    // the body topples onto the ground. Feet sit at y = -YPositionOffset.y in
    // post-scale local space.
    const float pivotY = YPositionOffset.y;
    glm::mat4 fallAroundFeet =
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -pivotY, 0.0f)) *
        glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0.0f, 0.0f, 1.0f)) *
        glm::translate(glm::mat4(1.0f), glm::vec3(0.0f,  pivotY, 0.0f));
    // Compose on top of the yaw rotation the manager just wrote.
    characterBodyParts.character.rotation =
        characterBodyParts.character.rotation * fallAroundFeet;
}

void Character::jumpAnimation(float dt)
{
    float jumpHeight = 2.0f;
    float jumpDuration = 0.7f;

    characterBodyParts.jumpPhase += dt;
    float t = characterBodyParts.jumpPhase / jumpDuration;

    if (t >= 1.0f)
    {
        // Remove any remaining jump offset
        characterBodyParts.character.translation -= glm::translate(glm::mat4(1.0f), glm::vec3(0, characterBodyParts.jumpOffset, 0));
        characterBodyParts.jumpOffset = 0.0f;
        characterBodyParts.onJumpAnimation = false;
        characterBodyParts.jumpPhase = 0.0f;
        return;
    }

    // Compute new offset
    float curve = -4.0f * (t - 0.5f) * (t - 0.5f) + 1.0f; // 0 → 1 → 0
    float newOffset = curve * jumpHeight;

    // Apply delta to translation
    float deltaOffset = newOffset - characterBodyParts.jumpOffset;
    characterBodyParts.character.translation = glm::translate(characterBodyParts.character.translation, glm::vec3(0, deltaOffset, 0));

    // Store current offset
    characterBodyParts.jumpOffset = newOffset;
}
