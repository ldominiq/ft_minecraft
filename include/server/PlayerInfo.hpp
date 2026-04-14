
#ifndef PLAYER_INFO_HPP
#define PLAYER_INFO_HPP

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netinet/in.h>
#endif
#include <string>
#include <vector>
#include <algorithm>
#include <cstdint>
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

		glm::vec3 startingPosition = glm::vec3(0.5, 150, 0.5);
		std::shared_ptr<PlayerMovement> movement = std::make_shared<PlayerMovement>(startingPosition);

		/// Spawns as close to [0,0] as possible on dry land.
		/// Checks radius 0 first (origin), then expands outward by 16 blocks each step.
		void computeSpawnPosition(const TerrainGenerationParams& params) {
			for (int radius = 0; radius <= 100; radius += 16) {
				for (int dx = -radius; dx <= radius; dx += 16) {
					for (int dz = -radius; dz <= radius; dz += 16) {
						const int surfaceY = ChunkGeneration::computeTerrainHeight(params, dx, dz);
						if (surfaceY > params.seaLevel) {
							startingPosition = glm::vec3(dx + 0.5f, surfaceY + 3, dz + 0.5f);
							movement->setPosition(startingPosition);
							movement->setYawAndPitch(0.0f, 0.0f);
							return;
						}
					}
				}
			}
			// Last resort: above the water at origin
			startingPosition = glm::vec3(0.5f, params.seaLevel + 3, 0.5f);
			movement->setPosition(startingPosition);
			movement->setYawAndPitch(0.0f, 0.0f);
		}
		std::vector<ChunkPos> rdyChunks;

		bool connected; //unused
};

#endif