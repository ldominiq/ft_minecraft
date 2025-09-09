
#ifndef PLAYER_INFO_HPP
#define PLAYER_INFO_HPP

#include <netinet/in.h>
#include <string>
#include <vector>
#include <glm/vec3.hpp>
#include <chrono>

#include "ChunkGeneration.hpp"
#include "Protocol.hpp"
#include "LivingEntity.hpp"

#define FLY_SPEED 50.0f
#define DEFAULT_SPEED 5.0f

enum class GAMEMODES {
	SURVIVAL = 0,
	SPECTATOR,
};

class CPlayerInfo final : public LivingEntity
{
	bool jumpBoostApplied = true;
	
	NetPlayerInputs lastInputsPktRecvd;
	GAMEMODES gamemode = GAMEMODES::SURVIVAL;
	float yaw, pitch;

	glm::vec3 getDesiredMove() override;
	void doJump(const std::unique_ptr<World> &world) override;

	void updatePosition();

	//TEST
	int number = 0;
	public:

		CPlayerInfo();

		int id;
		sockaddr_in addr;
		std::chrono::_V2::steady_clock::time_point lastPktRecvTick;

		std::string name;
		uint8_t loadRadius;

		glm::vec3 lastPositionSent;
		int health; //unused

		std::unordered_set<ChunkPos> loadedChunks;
		std::vector<ChunkPos> rdyChunks;

		bool connected; //unused

		inline const glm::vec3 getPosition() const { return position; }
		inline const glm::vec3 getCameraDir() const { return Front; }
		inline void setLastInputPacketReceived(NetPlayerInputs &pkt) {lastInputsPktRecvd = pkt; }

		void updateCameraVectors(float yaw, float pitch);
		void calculateNewPosition(const std::unique_ptr<World> &world) override;
};

#endif