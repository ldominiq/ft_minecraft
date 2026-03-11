
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

		int health; //unused

		glm::vec3 startingPosition = glm::vec3(0.5, 150, 0.5);
		std::shared_ptr<PlayerMovement> movement = std::make_shared<PlayerMovement>(startingPosition);

		// TODO: check if surfaceY is below seaLevel and if so, set starting Y to seaLevel + 1 to avoid drowning spawn
		// (when water physics is implemented) -> max(surfaceY, seaLevel) + offset)
		/// Set the starting position to the top of the terrain at the given spawn point.
		void computeSpawnPosition(const TerrainGenerationParams& params) {
			int surfaceY = ChunkGeneration::computeTerrainHeight(params, 0.0f, 0.0f);
			startingPosition = glm::vec3(0.5, surfaceY + 3, 0.5);
			movement->setPosition(startingPosition);
			movement->setYawAndPitch(0.0f, 0.0f);
		}

		std::unordered_set<ChunkPos> loadedChunks;
		std::vector<ChunkPos> rdyChunks;

		bool connected; //unused
};

#endif