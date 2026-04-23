
#include "ClientCreeper.hpp"
#include <algorithm>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

ClientCreeper::ClientCreeper(const glm::vec3 &position, float yaw, entityID ID)
	: Creeper(position, yaw, ID),
	  IClientEntity(position, yaw, ID),
	  Character(),
	  LivingEntity(position, yaw, ID)
{
	setPartsDimensions();
	createCharacterAt(position, entityWidth, entityHeight);
}

void ClientCreeper::setPartsDimensions()
{
	// Minecraft-creeper proportions in "pixel" units. X = depth (front/back),
	// Y = up, Z = width (side-to-side). Legs are thin so four of them can
	// sit at the four torso corners without overlapping.
	torsoScaleX = 8.0f;   // depth
	torsoScaleY = 12.0f;  // height
	torsoScaleZ = 8.0f;   // width

	headScaleX = 10.0f;
	headScaleY = 10.0f;
	headScaleZ = 10.0f;
	headTransY = (torsoScaleY * 0.5f + headScaleY * 0.5f) / headScaleY;

	armScaleX = armScaleY = armScaleZ = 0.0f;
	armTransZ = armTransY = 0.0f;

	// Four small square-cross-section legs — one at each torso corner, outside the body.
	legScaleX = 6.0f;
	legScaleY = 6.0f;
	legScaleZ = 6.0f;
	legTransY = -((torsoScaleY * 0.5f + legScaleY * 0.5f) / legScaleY);

	characterYScaleNorm = 1.0f / (torsoScaleY + headScaleY + legScaleY);
	characterXScaleNorm = 1.0f / headScaleX;
	characterZScaleNorm = 1.0f / headScaleX;

	feetPositionY = torsoScaleY * 0.5f + legScaleY;
	YPositionOffset = glm::vec3(0, feetPositionY * characterYScaleNorm, 0);
}

void ClientCreeper::createCharacterAt(const glm::vec3 &pos, float width, float height)
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

	// Torso
	auto torso = std::make_shared<Shape>(glm::vec3(0.2f, 0.75f, 0.2f));
	torsoBaseScale = glm::vec3(torsoScaleX, torsoScaleY, torsoScaleZ);
	torso->scale = glm::scale(glm::mat4(1.0f), torsoBaseScale);
	character.addChild(torso);

	// Head (darker green, slightly.)
	auto head = std::make_shared<Shape>(glm::vec3(0.15f, 0.55f, 0.15f));
	headBaseScale = glm::vec3(headScaleX, headScaleY, headScaleZ);
	head->scale = glm::scale(glm::mat4(1.0f), headBaseScale);
	head->translation = glm::translate(glm::mat4(1.0f), glm::vec3(0, headTransY, 0));
	character.addChild(head);

	// Four legs at the torso corners. Translation values are expressed in leg-local
	// units (Space composes as scale-then-translate internally — see Character.cpp for
	// the identical pattern on zombie legs).
	auto makeLeg = [&](float txLocal, float tzLocal) {
		auto leg = std::make_shared<Shape>(glm::vec3(0.18f, 0.6f, 0.18f));
		leg->scale = glm::scale(glm::mat4(1.0f), glm::vec3(legScaleX, legScaleY, legScaleZ));
		leg->translation = glm::translate(glm::mat4(1.0f), glm::vec3(txLocal, legTransY, tzLocal));
		character.addChild(leg);
		return leg;
	};

	// Place legs OUTSIDE the torso footprint: inner edge of leg touches outer edge of torso.
	// Translation is expressed in leg-local units (divide by leg scale so it lands in parent space).
	const float xOff = (torsoScaleX * 0.5f + legScaleX * 0.5f) / legScaleX;
	const float zOff = (torsoScaleZ * 0.5f + legScaleZ * 0.5f) / legScaleZ;

	auto frontRightLeg = makeLeg( xOff,  zOff);
	auto frontLeftLeg  = makeLeg( xOff, -zOff);
	auto backRight     = makeLeg(-xOff,  zOff);
	auto backLeft      = makeLeg(-xOff, -zOff);

	characterBodyParts.character = character;
	characterBodyParts.head  = head;
	characterBodyParts.torso = torso;
	// Creepers have no arms/forearms/calves; leave those members null. Overridden
	// swingArmAnimation / applyHeadPitch / walkAnimation never dereference them.
	characterBodyParts.rightLeg = frontRightLeg;
	characterBodyParts.leftLeg  = frontLeftLeg;
	backRightLeg = backRight;
	backLeftLeg  = backLeft;
}

void ClientCreeper::walkAnimation(float deltaTime)
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

	float angle = std::sin(characterBodyParts.walkPhase * 2.0f * static_cast<float>(M_PI)) * 0.4f;
	float legPivotY = -torsoScaleY / 2.0f;

	// Diagonal-pair trot: front-right + back-left move in phase; the other pair opposed.
	rotateBodyPart(characterBodyParts.rightLeg,  legPivotY,  angle);
	rotateBodyPart(backLeftLeg,                  legPivotY,  angle);
	rotateBodyPart(characterBodyParts.leftLeg,   legPivotY, -angle);
	rotateBodyPart(backRightLeg,                 legPivotY, -angle);
}

void ClientCreeper::tickFuseAnimation(float deltaTime)
{
	// Ease inflation toward 1 while triggered, back toward 0 otherwise.
	constexpr float INFLATE_DURATION = 1.5f;
	constexpr float DEFLATE_DURATION = 0.5f;
	constexpr float MAX_GROWTH       = 0.9f;

	if (clientPrimed)
		inflation += deltaTime / INFLATE_DURATION;
	else
		inflation -= deltaTime / DEFLATE_DURATION;
	inflation = std::clamp(inflation, 0.0f, 1.0f);

	// Wobble amplitude and frequency both scale with inflation^2 so the creeper
	// visibly strains harder as it nears the pop
	wobblePhase += deltaTime * (8.0f + 20.0f * inflation);
	float wobbleAmp = 0.12f * inflation * inflation;
	float growth    = 1.0f + MAX_GROWTH * inflation;
	float pulse     = growth + wobbleAmp * std::sin(wobblePhase);

	if (characterBodyParts.torso)
		characterBodyParts.torso->scale = glm::scale(glm::mat4(1.0f), torsoBaseScale * pulse);
	if (characterBodyParts.head)
		characterBodyParts.head->scale  = glm::scale(glm::mat4(1.0f), headBaseScale  * pulse);
}
