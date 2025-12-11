
#include "ClientZombie.hpp"

ClientZombie::ClientZombie(const glm::vec3 &position, float yaw, entityID ID) : Zombie(position, yaw, ID), IClientEntity(position,yaw,ID), Character(position), LivingEntity(position, yaw, ID)
{
	createCharacterAt(position, entityHeight);
}

void ClientZombie::createCharacterAt(const glm::vec3 &pos, float characterScale)
{
    Space character;

    // Zombie proportions (matching Minecraft: ~1.7 blocks tall total)
    const float bodyWidth = 0.4f;
    const float bodyHeight = 1.2f;
    const float bodyDepth = 0.4f;
    const float headSize = 0.5f;
    const float legWidth = 0.3f;
    const float legHeight = 0.4f;
    const float legDepth = 0.3f;
    const float legOffset = 0.7f; // Distance from body center to leg center (just outside body edge)
    // arms translation based on the 90deg rotation
    const float armTransZ = 1.5f;
    const float armTransY = 0.3f;
    const float armTransX = 0.8f;

    // Calculate total height for proper scaling
    const float totalHeight = torsoScaleY + headScaleY + legScaleY * 2.0f;
    
    // Override the inherited characterScaleNorm for zombie-specific proportions
    const float zombieScaleNorm = 1 / totalHeight;
    
    // Calculate Y offset so zombie feet are at ground level
    const float upTranslationRatio = (bodyHeight / 2.0f + legHeight) - (totalHeight / 2.0f);
    YPositionOffset = glm::vec3(0, ((upTranslationRatio * characterScale * zombieScaleNorm + totalHeight / 2.0f) * characterScale * zombieScaleNorm), 0);
    
    character.translation = glm::translate(glm::mat4(1.0f), pos + YPositionOffset);
    character.scale = glm::scale(glm::mat4(1.0f), glm::vec3(characterScale * zombieScaleNorm));

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
    rightArm->rotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0,0,1));
    rightArm->translation = glm::translate(glm::mat4(1.0f), glm::vec3(armTransX, -armTransY, armTransZ));
    character.addChild(rightArm);
    
    auto leftArm = std::make_shared<Shape>(glm::vec3(0,0,1));
    leftArm->scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, armScaleY, armScaleZ));
    leftArm->rotation = glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0,0,1));
    leftArm->translation = glm::translate(glm::mat4(1.0f), glm::vec3(armTransX, -armTransY, -armTransZ));
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

    // Store body parts
    characterBodyParts.character = character;
    characterBodyParts.head = head;
    characterBodyParts.torso = torso;
    characterBodyParts.rightLeg = rightLeg;
    characterBodyParts.leftLeg = leftLeg;
    characterBodyParts.rightCalf = rightCalf;
    characterBodyParts.leftCalf = leftCalf;

    characterBodyParts.rightArm = rightArm;
    characterBodyParts.leftArm = leftArm;
    characterBodyParts.rightForearm = rightForearm;
    characterBodyParts.leftForearm = leftForearm;
}

void ClientZombie::walkAnimation(float deltaTime)
{
    if (!characterBodyParts.rightLeg || !characterBodyParts.leftLeg || 
        !characterBodyParts.rightCalf || !characterBodyParts.leftCalf)
        return;

    const float walkSpeed = 4.0f;
    const float legSwing = glm::radians(15.0f); // Swing angle in radians
    
    float walkAmplitude = 1.0f;
    
    // Normalized cycle 0..1
    float norm = characterBodyParts.walkPhase - floor(characterBodyParts.walkPhase);
    characterBodyParts.normalizedWalkAnimationCycle = norm;

    characterBodyParts.walkPhase += deltaTime * walkSpeed;
    float swing = sin(characterBodyParts.walkPhase) * legSwing;
    // Angle for limbs
    float angle = sin(norm * 2.0f * M_PI) * walkAmplitude;

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

void ClientZombie::jumpAnimation(float deltaTime)
{
    // Zombies keep legs straight when jumping
    if (characterBodyParts.rightLeg) 
        characterBodyParts.rightLeg->rotation = glm::mat4(1.0f);
    if (characterBodyParts.leftLeg) 
        characterBodyParts.leftLeg->rotation = glm::mat4(1.0f);
    if (characterBodyParts.rightCalf) 
        characterBodyParts.rightCalf->rotation = glm::mat4(1.0f);
    if (characterBodyParts.leftCalf) 
        characterBodyParts.leftCalf->rotation = glm::mat4(1.0f);
}
