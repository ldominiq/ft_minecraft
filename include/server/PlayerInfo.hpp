
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

		int32_t id;
		int32_t serverClientReconciliationTick; // last sequence number received from this player, used for loss detection (possibly packet ordering..)

		sockaddr_in addr;

		std::string name;

		std::shared_ptr<PlayerMovement> movement = std::make_shared<PlayerMovement>();

		// TODO: check if surfaceY is below seaLevel and if so, set starting Y to seaLevel + 1 to avoid drowning spawn
		// (when water physics is implemented) -> max(surfaceY, seaLevel) + offset)
		/// Set the starting position to the top of the terrain at the given spawn point.
		void computeSpawnPosition(const TerrainGenerationParams& params) {
			int surfaceY = ChunkGeneration::computeTerrainHeight(params, 0.0f, 0.0f);
			movement->spawnPosition = glm::vec3(0.5, surfaceY + 3, 0.5);
			movement->setPosition(movement->spawnPosition);
			movement->setYawAndPitch(0.0f, 0.0f);
		}
		std::vector<ChunkPos> rdyChunks;

		bool connected; //unused
};

#endif