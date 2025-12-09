
#include "ClientCreeper.hpp"

ClientCreeper::ClientCreeper(const glm::vec3 &position, float yaw, entityID ID) : Creeper(position, yaw, ID), IClientEntity(position,yaw,ID), Character(position), LivingEntity(position, yaw, ID)
{
	createCharacterAt(position, entityHeight);
}

void ClientCreeper::createCharacterAt(const glm::vec3 &pos, float characterScale)
{
    Space character;

    // Creeper proportions (matching Minecraft: ~1.7 blocks tall total)
    const float bodyWidth = 0.4f;
    const float bodyHeight = 1.2f;
    const float bodyDepth = 0.4f;
    const float headSize = 0.5f;
    const float legWidth = 0.5f;
    const float legHeight = 0.5f;
    const float legDepth = 0.5f;
    const float legOffset = 0.7f; // Distance from body center to leg center (just outside body edge)

    // Calculate total height for proper scaling
    const float totalHeight = bodyHeight + headSize + legHeight;
    
    // Override the inherited characterScaleNorm for creeper-specific proportions
    const float creeperScaleNorm = 1.0f / totalHeight;
    
    // Calculate Y offset so creeper feet are at ground level
    const float upTranslationRatio = (bodyHeight / 2.0f + legHeight) - (totalHeight / 2.0f);
    YPositionOffset = glm::vec3(0, ((upTranslationRatio * characterScale * creeperScaleNorm + totalHeight / 2.0f) * characterScale * creeperScaleNorm), 0);
    
    character.translation = glm::translate(glm::mat4(1.0f), pos + YPositionOffset);
    character.scale = glm::scale(glm::mat4(1.0f), glm::vec3(characterScale * creeperScaleNorm));

    // Body (main torso - tall and rectangular)
    auto body = std::make_shared<Shape>(glm::vec3(255, 255, 225));
    body->scale = glm::scale(glm::mat4(1.0f), glm::vec3(bodyWidth, bodyHeight, bodyDepth));
    character.addChild(body);

    // Head (cube on top of body)
    auto head = std::make_shared<Shape>(glm::vec3(0, 255, 0));
    head->scale = glm::scale(glm::mat4(1.0f), glm::vec3(headSize, headSize, headSize));
    head->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, bodyHeight/2.0f + headSize/2.0f, 0));
    character.addChild(head);

    // Legs positioned at corners, pivot at body bottom, extend downward
    // Front-right leg (positive X, positive Z)
    auto frontRightLeg = std::make_shared<Shape>(glm::vec3(255, 0, 0));
    frontRightLeg->scale = glm::scale(glm::mat4(1.0f), glm::vec3(legWidth, legHeight, legDepth));
    // Position: body bottom edge (Y) + leg center offset (half leg height down)
    frontRightLeg->translation = glm::translate(glm::mat4(1.0f), 
        glm::vec3(legOffset, -bodyHeight/2.0f - legHeight/2.0f, legOffset));
    character.addChild(frontRightLeg);

    // Front-left leg (negative X, positive Z)
    auto frontLeftLeg = std::make_shared<Shape>(glm::vec3(225, 255, 0));
    frontLeftLeg->scale = glm::scale(glm::mat4(1.0f), glm::vec3(legWidth, legHeight, legDepth));
    frontLeftLeg->translation = glm::translate(glm::mat4(1.0f), 
        glm::vec3(-legOffset, -bodyHeight/2.0f - legHeight/2.0f, legOffset));
    character.addChild(frontLeftLeg);

    // Back-right leg (positive X, negative Z)
    auto backRightLeg = std::make_shared<Shape>(glm::vec3(0, 0, 255));
    backRightLeg->scale = glm::scale(glm::mat4(1.0f), glm::vec3(legWidth, legHeight, legDepth));
    backRightLeg->translation = glm::translate(glm::mat4(1.0f), 
        glm::vec3(legOffset, -bodyHeight/2.0f - legHeight/2.0f, -legOffset));
    character.addChild(backRightLeg);

    // Back-left leg (negative X, negative Z)
    auto backLeftLeg = std::make_shared<Shape>(glm::vec3(0, 255, 255));
    backLeftLeg->scale = glm::scale(glm::mat4(1.0f), glm::vec3(legWidth, legHeight, legDepth));
    backLeftLeg->translation = glm::translate(glm::mat4(1.0f), 
        glm::vec3(-legOffset, -bodyHeight/2.0f - legHeight/2.0f, -legOffset));
    character.addChild(backLeftLeg);

    // Store body parts
    characterBodyParts.character = character;
    characterBodyParts.head = head;
    characterBodyParts.torso = body;
    characterBodyParts.rightLeg = frontRightLeg;
    characterBodyParts.leftLeg = frontLeftLeg;
    characterBodyParts.rightCalf = backRightLeg;
    characterBodyParts.leftCalf = backLeftLeg;
    
    // Set unused parts to nullptr
    characterBodyParts.rightArm = nullptr;
    characterBodyParts.leftArm = nullptr;
    characterBodyParts.rightForearm = nullptr;
    characterBodyParts.leftForearm = nullptr;
}

void ClientCreeper::walkAnimation(float deltaTime)
{
    if (!characterBodyParts.rightLeg || !characterBodyParts.leftLeg || 
        !characterBodyParts.rightCalf || !characterBodyParts.leftCalf)
        return;

    const float walkSpeed = 5.0f;
    const float legSwing = glm::radians(30.0f); // Swing angle in radians
    
    characterBodyParts.walkPhase += deltaTime * walkSpeed;
    float swing = sin(characterBodyParts.walkPhase) * legSwing;

    // Creeper quadruped gait: diagonal pairs move together
    // Front-right + back-left move forward, front-left + back-right move backward
    // Rotate around X axis (forward/backward swing in Z direction)
    characterBodyParts.rightLeg->rotation = glm::rotate(glm::mat4(1.0f), swing, glm::vec3(0, 0, 1));
    characterBodyParts.leftLeg->rotation = glm::rotate(glm::mat4(1.0f), -swing, glm::vec3(0, 0, 1));
    characterBodyParts.rightCalf->rotation = glm::rotate(glm::mat4(1.0f), -swing, glm::vec3(0, 0, 1));
    characterBodyParts.leftCalf->rotation = glm::rotate(glm::mat4(1.0f), swing, glm::vec3(0, 0, 1));
}

void ClientCreeper::jumpAnimation(float deltaTime)
{
    // Creepers keep legs straight when jumping
    if (characterBodyParts.rightLeg) 
        characterBodyParts.rightLeg->rotation = glm::mat4(1.0f);
    if (characterBodyParts.leftLeg) 
        characterBodyParts.leftLeg->rotation = glm::mat4(1.0f);
    if (characterBodyParts.rightCalf) 
        characterBodyParts.rightCalf->rotation = glm::mat4(1.0f);
    if (characterBodyParts.leftCalf) 
        characterBodyParts.leftCalf->rotation = glm::mat4(1.0f);
}
