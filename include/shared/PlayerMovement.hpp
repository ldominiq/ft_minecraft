
#ifndef PLAYER_MOVEMENT_HPP
#define PLAYER_MOVEMENT_HPP

#include "LivingEntity.hpp"
#include "Protocol.hpp"

#define FLY_SPEED 50.0f
#define DEFAULT_SPEED 5.0f

enum class GAMEMODES {
	SURVIVAL = 0,
	SPECTATOR,
};

struct PlayerMovement final : public LivingEntity {

	NetPlayerInputs lastInputsPktRecvd = {};

	bool jumpBoostApplied = false;

	GAMEMODES gamemode = GAMEMODES::SPECTATOR;
	int32_t tick;

	glm::vec3 getDesiredMove() override;
	void doJump(const ICommonWorld &world) override;

	void updatePosition();

	void updateCameraVectors();
	void calculateNewPosition(const ICommonWorld &world) override;

	inline const glm::vec3 getVelocity() const { return this->velocity; }
	inline const float getVerticalVelocity() const { return this->verticalVelocity; }
	inline const glm::vec3 getCameraDir() const { return this->Front; }
	inline const int32_t getTick() const { return tick; }

	inline void setVelocity(glm::vec3 velocity) {this->velocity = velocity; }
	inline void setVerticalVelocity(float verticalVelocity) {this->verticalVelocity = verticalVelocity; }
	inline void setYawAndPitch(float yaw, float pitch) {this->yaw = yaw, this->pitch = pitch; }

	PlayerMovement();
	~PlayerMovement();
};

#endif
