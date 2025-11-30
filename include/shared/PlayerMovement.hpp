
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

	glm::vec3 getDesiredMove() override;
	void doJump(const ICommonWorld &world) override;

	void updatePosition();

	void updateCameraVectors();
	void calculateNewPosition(const ICommonWorld &world) override;

	inline const glm::vec3 getVelocity() const { return this->velocity; }
	inline const glm::vec3 getCameraDir() const { return this->Front; }
	inline const float getYaw() const { return yaw; }
	inline const float getPitch() const { return pitch; }

	inline void setVelocity(glm::vec3 velocity) {this->velocity = velocity; }
	inline void setYawAndPitch(float yaw, float pitch) {this->yaw = yaw, this->pitch = pitch; }
	inline void setLastInputPacketReceived(NetPlayerInputs &pkt) {lastInputsPktRecvd = pkt; }
	inline void setGamemode(GAMEMODES mode) {gamemode = mode; }

	PlayerMovement();
	~PlayerMovement();
};

#endif
