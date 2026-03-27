
#include "Character.hpp"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

Character::Character()
{
}

void Character::setPartsDimensions()
{
	torsoScaleZ = 2.4f;
	torsoScaleY = 5.0f;

	headScaleZ = torsoScaleZ*1.2f;
	headScaleY = headScaleZ;
	headTransY = (torsoScaleY/2.0f + headScaleY/2.0f) / headScaleY;

	armScaleZ = torsoScaleZ/2.0f;
	armScaleY = (torsoScaleY*7.0f)/11.0f;
	armTransZ = (torsoScaleZ/2.0f + armScaleZ/2.0f) / armScaleZ;
	armTransY = ((torsoScaleY - armScaleY)/2.0f) / armScaleY;

	legScaleZ = torsoScaleZ/2.0f;
	legScaleY = torsoScaleY*0.6f;
	legTransZ = 0.5f;
	legTransY = -((torsoScaleY/2.0f + legScaleY/2.0f) / legScaleY); // 0.875 = 3 (torso Y scale) / 2 (Y negative/positive) = 1.5, 4 (rightLeg Y scale) / 2 (Y negative/positive). 1.5+2 / 4 (rightleg Y scale as translation goes scale times fast)

	characterYScaleNorm = 1.0f / (torsoScaleY + headScaleY + legScaleY * 2.0f);
	characterZScaleNorm = 1.0f / (torsoScaleZ + armScaleZ * 2.0f);
	characterXScaleNorm = 1.0f / 1.0f;

	feetPositionY = torsoScaleY/2.0f + legScaleY * 2.0f;
	YPositionOffset = glm::vec3(0, feetPositionY * characterYScaleNorm, 0);
}

// ...existing code...
void Character::createCharacterAt(const glm::vec3 &pos, float width, float height)
{
    Space character;

	width = width * 1.5f;
	characterXScaleNorm = (width / 2.0f);
	character.scale = glm::scale(glm::mat4(1.0f), glm::vec3(characterXScaleNorm, height * characterYScaleNorm, width * characterZScaleNorm));
	YPositionOffset = glm::vec3(0, feetPositionY * characterYScaleNorm * height, 0);
	character.translation = glm::translate(glm::mat4(1.0f), pos + YPositionOffset);

    // Torso
    auto torso = std::make_shared<Shape>(glm::vec3(1,0,0));
    torso->scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, torsoScaleY, torsoScaleZ));
    character.addChild(torso);

    // Head
    auto head = std::make_shared<Shape>(glm::vec3(0,1,0));
    head->scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, headScaleY, headScaleZ));
    head->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, headTransY, 0));
    character.addChild(head);

    // Arms
    auto rightArm = std::make_shared<Shape>(glm::vec3(0,0,1));
    rightArm->scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, armScaleY, armScaleZ));
    rightArm->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, armTransY, armTransZ));
    character.addChild(rightArm);

    auto leftArm = std::make_shared<Shape>(glm::vec3(0,0,1));
    leftArm->scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, armScaleY, armScaleZ));
    leftArm->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, armTransY, -armTransZ));
    character.addChild(leftArm);

    // Forearms
    auto rightForearm = std::make_shared<Shape>(glm::vec3(0.5f,0,1));
    rightForearm->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, 0));
    rightArm->addChild(rightForearm);

    auto leftForearm = std::make_shared<Shape>(glm::vec3(0.5f,0,1));
    leftForearm->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, 0));
    leftArm->addChild(leftForearm);

    // Legs
    auto rightLeg = std::make_shared<Shape>(glm::vec3(1,1,0));
    rightLeg->scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, legScaleY, legScaleZ));
    rightLeg->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, legTransY, legTransZ));
    character.addChild(rightLeg);

    auto leftLeg = std::make_shared<Shape>(glm::vec3(1,1,0));
    leftLeg->scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, legScaleY, legScaleZ));
    leftLeg->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, legTransY, -legTransZ));
    character.addChild(leftLeg);

    // Calves
    auto rightCalf = std::make_shared<Shape>(glm::vec3(1,1,0.5f));
    rightCalf->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, 0));
    rightLeg->addChild(rightCalf);

    auto leftCalf = std::make_shared<Shape>(glm::vec3(1,1,0.5f));
    leftCalf->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, 0));
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

void Character::rotateBodyPart(const std::shared_ptr<Shape>& bodyPart, float pivot, float angle)
{
	bodyPart->rotation =
		glm::translate(glm::mat4(1.0f), glm::vec3(0, pivot, 0)) *
		extractScaleInverse(characterBodyParts.character.scale) *
		glm::rotate(glm::mat4(1.0f), angle * 0.8f, glm::vec3(0,0,1)) *
		characterBodyParts.character.scale *
		glm::translate(glm::mat4(1.0f), glm::vec3(0, -pivot, 0));
}

void Character::walkAnimation(float deltaTime)
{
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
	float pivotY = +armScaleY * 0.5f;

	rotateBodyPart(characterBodyParts.leftArm, pivotY, angle);
	rotateBodyPart(characterBodyParts.rightArm, pivotY, -angle);

	if (angle >= 0)
		rotateBodyPart(characterBodyParts.leftForearm, -1.5f, angle * 1.2f);

	if (angle <= 0)
		rotateBodyPart(characterBodyParts.rightForearm, -1.5f, angle * -1.2f);

	// ---- Legs ----
	float legPivotY = -legScaleY * 0.5f;

	rotateBodyPart(characterBodyParts.leftLeg, legPivotY, -angle * 0.8f);
	rotateBodyPart(characterBodyParts.rightLeg, legPivotY, angle * 0.8f);
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
