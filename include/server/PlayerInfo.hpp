
#ifndef PLAYER_INFO_HPP
#define PLAYER_INFO_HPP

#include <netinet/in.h>
#include <string>
#include <vector>
#include <glm/vec3.hpp>
#include <chrono>

#include "ChunkGeneration.hpp"
#include "PlayerMovement.hpp"

class CPlayerInfo
{
	PlayerMovement<ChunkGeneration> movement;

	public:

		CPlayerInfo();

		int id;
		sockaddr_in addr;
		std::chrono::_V2::steady_clock::time_point lastPktRecvTick;

		std::string name;
		uint8_t loadRadius; // TODO : set setter on new packet

		glm::vec3 lastPositionSent;
		int health; //unused

		std::unordered_set<ChunkPos> loadedChunks;
		std::vector<ChunkPos> rdyChunks;

		bool connected; //unused

		inline const glm::vec3 getPosition() const { return movement.getPosition(); }
		inline const glm::vec3 getVelocity() const { return movement.getVelocity(); }
		inline const int32_t getTick() const { return movement.getTick(); }
		inline const float getVerticalVelocity() const { return movement.getVerticalVelocity(); }

		inline const glm::vec3 getCameraDir() const { return movement.getCameraDir(); }
		inline void setLastInputPacketReceived(NetPlayerInputs &pkt) {movement.lastInputsPktRecvd = pkt; }
		inline void setGamemode(GAMEMODES gamemode) {movement.gamemode = gamemode; }

		inline void updateCameraVectors() {movement.updateCameraVectors(); }
		inline void setYawAndPitch(float yaw, float pitch) {movement.setYawAndPitch(yaw, pitch); }
		inline void calculateNewPosition(const CommonWorld<ChunkGeneration> &world) {movement.calculateNewPosition(world); }
};

#endif