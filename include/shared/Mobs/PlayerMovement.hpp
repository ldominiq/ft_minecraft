
#ifndef PLAYER_MOVEMENT_HPP
#define PLAYER_MOVEMENT_HPP

#include "LivingEntity.hpp"
#include "Protocol.hpp"
#include "PlayerInventory.hpp"
#include "CraftingStation.hpp"
#include "Inventory.hpp"
#include <deque>

#define FLY_SPEED 50.0f
#define DEFAULT_SPEED 5.0f

enum class GAMEMODES {
	SURVIVAL = 0,
	SPECTATOR,
};

struct PlayerMovement : public virtual LivingEntity {

	NetPlayerInputs lastInputsPktRecvd = {};

	std::shared_ptr<InventoryExternalVariablesRefs> inventoryExternalVarsRefs = std::make_shared<InventoryExternalVariablesRefs>();
	std::shared_ptr<PlayerInventory> inventory = std::make_shared<PlayerInventory>(inventoryExternalVarsRefs);
	std::shared_ptr<CraftingStation> craftingStation = std::make_shared<CraftingStation>(inventoryExternalVarsRefs);

	bool jumpBoostApplied = false;
    int32_t lastAppliedServerClientReconciliationTick = -1; // -1 is the default "no input yet" client tick
	bool skipDuplicateInputs = false; // set true on server to prevent re-applying stale inputs
	std::deque<NetPlayerInputs> pendingInputs; // server-side queue: one physics step per entry
	uint8_t loadRadius = 16;

	GAMEMODES gamemode = GAMEMODES::SPECTATOR;

	glm::vec3 spawnPosition{};

	glm::vec3 getDesiredMove() override;
	void doJump(const ICommonWorld &world) override;

	void onDeath() override;

	void calculateUnderwaterPosition(const ICommonWorld &world) override;
	void updatePosition();
	void updateCameraVectors();
	void applyFallDamage() override;
	void calculateNewPosition(const ICommonWorld &world) override;

	inline const glm::vec3 getVelocity() const { return this->velocity; }
	inline const glm::vec3 getCameraDir() const { return this->Front; }
	inline const float getYaw() const { return yaw; }
	inline const float getPitch() const { return pitch; }
	inline const uint8_t getLoadRadius() const { return loadRadius; }

	inline const std::shared_ptr<std::vector<draggedSlotInfo>> getDraggedSlots() const { return inventoryExternalVarsRefs->draggedSlots; }

	inline bool getJumpBoostApplied() const { return jumpBoostApplied; }
	inline int32_t getLastAppliedServerClientReconciliationTick() const { return lastAppliedServerClientReconciliationTick; }

	inline void setVelocity(glm::vec3 velocity) {this->velocity = velocity; }
	inline void setYawAndPitch(float yaw, float pitch) {this->yaw = yaw, this->pitch = pitch; }
	inline void setLastInputPacketReceived(NetPlayerInputs &pkt) {lastInputsPktRecvd = pkt; }
	inline void setGamemode(GAMEMODES mode) {gamemode = mode; }
	inline void setLoadRadius(uint8_t radius) { loadRadius = radius; }
	inline void setJumpBoostApplied(bool value) { jumpBoostApplied = value; }

	PlayerMovement();
	PlayerMovement(const glm::vec3 &position, float yaw, entityID ID);
	virtual ~PlayerMovement();
};

#endif
