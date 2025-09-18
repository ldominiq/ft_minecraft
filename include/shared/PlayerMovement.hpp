
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

template <typename ChunkT>
struct PlayerMovement final : public LivingEntity<ChunkT> {

	NetPlayerInputs lastInputsPktRecvd = {};

	bool jumpBoostApplied = false;

	GAMEMODES gamemode = GAMEMODES::SPECTATOR;
	float yaw, pitch;
	int32_t tick;

	glm::vec3 getDesiredMove() override;
	void doJump(const CommonWorld<ChunkT> &world) override;

	void updatePosition();

	void updateCameraVectors();
	void calculateNewPosition(const CommonWorld<ChunkT> &world) override;

	inline const glm::vec3 getPosition() const { return this->position; }
	inline const glm::vec3 getVelocity() const { return this->velocity; }
	inline const float getVerticalVelocity() const { return this->verticalVelocity; }
	inline const glm::vec3 getCameraDir() const { return this->Front; }
	inline const int32_t getTick() const { return tick; }

	inline void setPosition(glm::vec3 position) {this->position = position; }
	inline void setVelocity(glm::vec3 velocity) {this->velocity = velocity; }
	inline void setVerticalVelocity(float verticalVelocity) {this->verticalVelocity = verticalVelocity; }
	inline void setYawAndPitch(float yaw, float pitch) {this->yaw = yaw, this->pitch = pitch; }

	PlayerMovement();
	~PlayerMovement();
};

#include "PlayerMovement.inl"

#endif
