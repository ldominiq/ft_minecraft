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

		float walkPhase = 0;

		bool onJumpAnimation = false;
		float jumpPhase = 0;
		float jumpOffset = 0;
	};

	protected:
		float torsoScaleZ{0};
		float torsoScaleY{0};

		float headScaleZ{0};
		float headScaleY{0};
		float headTransY{0};

		float armScaleZ{0};
		float armScaleY{0};
		float armTransZ{0};
		float armTransY{0};

		float legScaleZ{0};
		float legScaleY{0};
		float legTransZ{0};
		float legTransY{0};

		float characterXScaleNorm{0};
		float characterYScaleNorm{0};
		float characterZScaleNorm{0};
		float feetPositionY{0};

	virtual void setPartsDimensions();
	virtual void createCharacterAt(const glm::vec3 &pos, float width, float height);
	void rotateBodyPart(const std::shared_ptr<Shape>& bodyPart, float pivot, float angle) const;

	public:
		s_character characterBodyParts;

		glm::vec3 YPositionOffset = {};

		Character();
		virtual ~Character() = default;

		virtual void walkAnimation(float deltaTime);
		virtual void jumpAnimation(float currentFrame);

};

#endif