
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
	public:

		CPlayerInfo();

		int id;
		sockaddr_in addr;

		std::string name;
		uint8_t loadRadius; // TODO : set setter on new packet

		glm::vec3 spawnPosition = glm::vec3(0,150,0);
		std::shared_ptr<PlayerMovement> movement = std::make_shared<PlayerMovement>(spawnPosition);

		std::unordered_set<ChunkPos> loadedChunks;
		std::vector<ChunkPos> rdyChunks;

		bool connected; //unused
};

#endif