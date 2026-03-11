#ifndef CHARACTER_HPP
#define CHARACTER_HPP

#include "LivingEntity.hpp"
#include "cube.hpp"
#include "Space.hpp"
#include "Shape.hpp"

//TODO make player, mobs etc inherit from this instead of just using this
class Character
{
	struct s_character
	{
		Space character;
		std::shared_ptr<Shape> head;
		std::shared_ptr<Shape> torso;
		std::shared_ptr<Shape> rightArm;
		std::shared_ptr<Shape> leftArm;
		std::shared_ptr<Shape> rightForearm;
		std::shared_ptr<Shape> leftForearm;
		std::shared_ptr<Shape> rightLeg;
		std::shared_ptr<Shape> leftLeg;
		std::shared_ptr<Shape> rightCalf;
		std::shared_ptr<Shape> leftCalf;

		bool onWalkAnimation = false;
		float walkingAnimationSpeed = 0.8;
		float walkAnimationFrameStart = 0;
		float normalizedWalkAnimationCycle = 0;

		float walkPhase = 0;

		bool onJumpAnimation = false;
		float jumpPhase = 0;
		float jumpOffset = 0;
	};

	bool doDraw = true;

	protected:
		float torsoScaleZ = 2.4f;
		float torsoScaleY = 5.0f;

		float headScaleZ = torsoScaleZ*1.2f;
		float headScaleY = headScaleZ;
		float headTransY = (torsoScaleY/2.0f + headScaleY/2.0f) / headScaleY;

		float armScaleZ = torsoScaleZ/2.0f;
		float armScaleY = (torsoScaleY*7.0f)/11.0f;
		float armTransZ = (torsoScaleZ/2.0f + armScaleZ/2.0f) / armScaleZ;
		float armTransY = ((torsoScaleY - armScaleY)/2.0f) / armScaleY;

		float legScaleZ = torsoScaleZ/2.0f;
		float legScaleY = torsoScaleY*0.6f;
		float legTransZ = 0.5f;
		float legTransY = -((torsoScaleY/2.0f + legScaleY/2.0f) / legScaleY); // 0.875 = 3 (torso Y scale) / 2 (Y negative/positive) = 1.5, 4 (rightLeg Y scale) / 2 (Y negative/positive). 1.5+2 / 4 (rightleg Y scale as translation goes scale times fast)

		float characterScaleNorm = 1 / (torsoScaleY + headScaleY + legScaleY * 2.0f);

	virtual void createCharacterAt(const glm::vec3 &pos, float characterScale);
	public:
		s_character characterBodyParts;

		glm::vec3 YPositionOffset = {};

		inline void setDoDraw(bool value) {doDraw = value;}
		inline bool DoDraw() const {return doDraw;}

		Character(const glm::vec3 &position);
		virtual ~Character() = default;

		virtual void walkAnimation(float deltaTime);
		virtual void jumpAnimation(float currentFrame);

};

#endif