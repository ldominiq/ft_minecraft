
#ifndef CLIENT_CREEPER_HPP
#define CLIENT_CREEPER_HPP

#include "IClientEntity.hpp"
#include "Creeper.hpp"

class ClientCreeper : public Creeper, public IClientEntity
{
		public :

		ClientCreeper(const glm::vec3 &position, float yaw, entityID ID);
		~ClientCreeper() = default;

		void walkAnimation(float deltaTime) override;
		void swingArmAnimation(float /*deltaTime*/) override {} // no arms
		void applyHeadPitch(float /*pitchDegrees*/) override {}  // creeper head is fixed

		// Inflate/deflate the body every frame, independent of movement, so the
		// fuse animation keeps advancing while the creeper stands still.
		void tickFuseAnimation(float deltaTime);

		// Visible primed (fused) state — client toggles from packet flag, draw pulses the body.
		bool clientPrimed = false;

		// Smoothed 0..1 inflation; drives torso/head scale and wobble.
		float inflation = 0.0f;

	protected:
		void setPartsDimensions() override;
		void createCharacterAt(const glm::vec3 &pos, float width, float height) override;

	private:
		// Extra legs (creeper has 4). Front pair reuses the shared struct's leftLeg/rightLeg.
		std::shared_ptr<Shape> backLeftLeg;
		std::shared_ptr<Shape> backRightLeg;

		// Unscaled baseline torso/head scales so the primed pulse can be applied per-frame.
		glm::vec3 torsoBaseScale = glm::vec3(1.0f);
		glm::vec3 headBaseScale  = glm::vec3(1.0f);

		float wobblePhase = 0.0f;
};

#endif
