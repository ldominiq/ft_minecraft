
#include "ClientCreeper.hpp"

ClientCreeper::ClientCreeper(const glm::vec3 &position, float yaw, entityID ID) : Creeper(position, yaw, ID), IClientEntity(position,yaw,ID), Character(position), LivingEntity(position, yaw, ID)
{
	createCharacterAt(position, entityHeight);
}

void ClientCreeper::createCharacterAt(const glm::vec3 &pos, float characterScale)
{
    Space character;

	float characterHeight = torsoScaleY + headScaleY + legScaleY * 2.0f;
	float upTranslationRatio = (torsoScaleY / 2.0f + legScaleY * 2.0f) - ((characterHeight) / 2.0f);
	YPositionOffset = glm::vec3(0, ((upTranslationRatio * characterScale * characterScaleNorm + characterHeight/2.0f) * characterScale * characterScaleNorm), 0);
    character.translation = glm::translate(glm::mat4(1.0f), pos + YPositionOffset);
	character.scale = glm::scale(glm::mat4(1.0f), glm::vec3(characterScale * characterScaleNorm));

    // Torso
    auto torso = std::make_shared<Shape>(glm::vec3(0, 255, 0));
    torso->scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, torsoScaleY, torsoScaleZ));
    character.addChild(torso);

    // Head
    auto head = std::make_shared<Shape>(glm::vec3(0, 255, 0));
    head->scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, headScaleY, headScaleZ));
    head->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, headTransY, 0));
    character.addChild(head);

    // Arms
    auto rightArm = std::make_shared<Shape>(glm::vec3(0, 255, 0));
    rightArm->scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, armScaleY, armScaleZ));
    rightArm->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, armTransY, armTransZ));
    character.addChild(rightArm);

    auto leftArm = std::make_shared<Shape>(glm::vec3(0, 255, 0));
    leftArm->scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, armScaleY, armScaleZ));
    leftArm->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, armTransY, -armTransZ));
    character.addChild(leftArm);

    // Forearms
    auto rightForearm = std::make_shared<Shape>(glm::vec3(0, 255, 0));
    rightForearm->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, 0));
    rightArm->addChild(rightForearm);

    auto leftForearm = std::make_shared<Shape>(glm::vec3(0, 255, 0));
    leftForearm->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, 0));
    leftArm->addChild(leftForearm);

    // Legs
    auto rightLeg = std::make_shared<Shape>(glm::vec3(0, 255, 0));
    rightLeg->scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, legScaleY, legScaleZ));
    rightLeg->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, legTransY, legTransZ));
    character.addChild(rightLeg);

    auto leftLeg = std::make_shared<Shape>(glm::vec3(0, 255, 0));
    leftLeg->scale = glm::scale(glm::mat4(1.0f), glm::vec3(1.0f, legScaleY, legScaleZ));
    leftLeg->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, legTransY, -legTransZ));
    character.addChild(leftLeg);

    // Calves
    auto rightCalf = std::make_shared<Shape>(glm::vec3(0, 255, 0));
    rightCalf->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, -1, 0));
    rightLeg->addChild(rightCalf);

    auto leftCalf = std::make_shared<Shape>(glm::vec3(0, 255, 0));
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