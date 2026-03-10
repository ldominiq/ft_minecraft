
#ifndef PLAYER_MOVEMENT_HPP
#define PLAYER_MOVEMENT_HPP

#include "LivingEntity.hpp"
#include "Protocol.hpp"
#include "Inventory.hpp"

#define FLY_SPEED 50.0f
#define DEFAULT_SPEED 5.0f

enum class GAMEMODES {
	SURVIVAL = 0,
	SPECTATOR,
};

struct PlayerMovement : public virtual LivingEntity {

	NetPlayerInputs lastInputsPktRecvd = {};

	Inventory inventory;

	bool jumpBoostApplied = false;
	uint8_t loadRadius = 16;

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
	inline const uint8_t getLoadRadius() const { return loadRadius; }

	inline void setVelocity(glm::vec3 velocity) {this->velocity = velocity; }
	inline void setYawAndPitch(float yaw, float pitch) {this->yaw = yaw, this->pitch = pitch; }
	inline void setLastInputPacketReceived(NetPlayerInputs &pkt) {lastInputsPktRecvd = pkt; }
	inline void setGamemode(GAMEMODES mode) {gamemode = mode; }
	inline void setLoadRadius(uint8_t radius) { loadRadius = radius; }

	PlayerMovement(const glm::vec3 &position);
	PlayerMovement(const glm::vec3 &position, float yaw, entityID ID);
	virtual ~PlayerMovement();
};

#endif
