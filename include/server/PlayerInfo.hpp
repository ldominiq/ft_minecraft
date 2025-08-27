
#ifndef PLAYER_INFO_HPP
#define PLAYER_INFO_HPP

#include <netinet/in.h>
#include <string>
#include <vector>
#include <glm/vec3.hpp>
#include "Chunk.hpp"
#include "Protocol.hpp"
#include <chrono>

#define ACCEL 100

class CPlayerInfo
{
	//camera
	glm::vec3 position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

	float yaw, pitch;
	float movementSpeed;

	void updateCameraVectors();

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

		void updatePosition(NetPlayerInputs &inputs, float &deltaTime);
		inline const glm::vec3 getPosition() const { return position; }
		inline const glm::vec3 getCameraDir() const { return Front; }
};

#endif