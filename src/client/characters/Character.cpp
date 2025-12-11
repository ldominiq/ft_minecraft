
#include "Character.hpp"

Character::Character(const glm::vec3 &position)
{
}

void Character::createCharacterAt(const glm::vec3 &pos, float characterScale)
{
	Space character;

	//when building the character. First goes the torso which is centered in the middle. But then legs go under it. This next variable helps recentering the whole character with his center being at 0.0.0
	//these calculations make no sense. But for now it gives the impression that it works... (only the .scale is good)
	float upTranslationRatio = (torsoScaleY + headScaleY + legScaleY * 2.0f) / (torsoScaleY / 2.0f + legScaleY * 2.0f);
	YPositionOffset = glm::vec3(0, -(characterScale * characterScaleNorm * upTranslationRatio * 2), 0);
    character.translation = glm::translate(glm::mat4(1.0f), pos + YPositionOffset);
	character.scale = glm::scale(glm::mat4(1.0f), glm::vec3(characterScale * characterScaleNorm));

    // Torso
    auto torso = std::make_shared<Shape>(glm::vec3(1,0,0));
    torso->scale = glm::scale(glm::mat4(1.0f), glm::vec3(torsoScaleZ / 2.0f, torsoScaleY, torsoScaleZ));
    character.addChild(torso);

    // Head
    auto head = std::make_shared<Shape>(glm::vec3(0,1,0));
    head->scale = glm::scale(glm::mat4(1.0f), glm::vec3(headScale));
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

// void Character::walkAnimation(float deltaTime)
// {
//     c.onWalkAnimation = true;
//     float walkAmplitude = 1.0f;

//     // 1. Update phase independent of speed
//     c.walkPhase += deltaTime * c.walkingAnimationSpeed;

//     // 2. Get normalized cycle (0..1)
//     float prevNorm = c.normalizedWalkAnimationCycle;
//     float norm = fmod(c.walkPhase, 1.0f);
//     c.normalizedWalkAnimationCycle = norm;

//     // 3. Detect cycle restart
//     if ((prevNorm > 0.9f && norm < 0.1f) ||
//         ((prevNorm < 0.5f && norm > 0.5f)))
//     {
//         c.normalizedWalkAnimationCycle = 0;
//         c.onWalkAnimation = false;
//     }

//     // 4. Compute angle
//     float angle = sin(c.normalizedWalkAnimationCycle * 2.0f * M_PI) * walkAmplitude;

void Character::walkAnimation(float deltaTime)
{
    float walkAmplitude = 1.0f;

    characterBodyParts.onWalkAnimation = true;

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

    // Normalized cycle 0..1
    float norm = characterBodyParts.walkPhase - floor(characterBodyParts.walkPhase);
    characterBodyParts.normalizedWalkAnimationCycle = norm;

    // Angle for limbs
    float angle = sin(norm * 2.0f * M_PI) * walkAmplitude;

    // ---- Arms ----
    float pivotY = +armScaleY * 0.5f;

    characterBodyParts.leftArm->rotation =
        glm::translate(glm::mat4(1.0f), glm::vec3(0, pivotY, 0)) *
        glm::rotate(glm::mat4(1.0f), angle, glm::vec3(0,0,1)) *
        glm::translate(glm::mat4(1.0f), glm::vec3(0, -pivotY, 0));

    characterBodyParts.rightArm->rotation =
        glm::translate(glm::mat4(1.0f), glm::vec3(0, pivotY, 0)) *
        glm::rotate(glm::mat4(1.0f), -angle, glm::vec3(0,0,1)) *
        glm::translate(glm::mat4(1.0f), glm::vec3(0, -pivotY, 0));

    if (angle >= 0)
    {
        characterBodyParts.leftForearm->rotation =
            glm::translate(glm::mat4(1.0f), glm::vec3(0, -1.5f, 0)) *
            glm::rotate(glm::mat4(1.0f), angle * 1.2f, glm::vec3(0,0,1)) *
            glm::translate(glm::mat4(1.0f), glm::vec3(0, 1.5f, 0));
    }

    if (angle <= 0)
    {
        characterBodyParts.rightForearm->rotation =
            glm::translate(glm::mat4(1.0f), glm::vec3(0, -1.5f, 0)) *
            glm::rotate(glm::mat4(1.0f), angle * -1.2f, glm::vec3(0,0,1)) *
            glm::translate(glm::mat4(1.0f), glm::vec3(0, 1.5f, 0));
    }

    // ---- Legs ----
    float legPivotY = -legScaleY * 0.5f;

    characterBodyParts.leftLeg->rotation =
        glm::translate(glm::mat4(1.0f), glm::vec3(0, legPivotY, 0)) *
        glm::rotate(glm::mat4(1.0f), -angle * 0.8f, glm::vec3(0,0,1)) *
        glm::translate(glm::mat4(1.0f), glm::vec3(0, -legPivotY, 0));

    characterBodyParts.rightLeg->rotation =
        glm::translate(glm::mat4(1.0f), glm::vec3(0, legPivotY, 0)) *
        glm::rotate(glm::mat4(1.0f), angle * 0.8f, glm::vec3(0,0,1)) *
        glm::translate(glm::mat4(1.0f), glm::vec3(0, -legPivotY, 0));
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
