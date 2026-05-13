
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
#include "Network.hpp"

class CPlayerInfo
{
	public:

		CPlayerInfo();

		int32_t id;
     int32_t serverClientReconciliationTick = -1; // last sequence number received from this player, used for loss detection (possibly packet ordering..)

		sockaddr_in addr;

		std::string originalName; // for name conflict resolution on join

		std::shared_ptr<PlayerMovement> movement = std::make_shared<PlayerMovement>();

		/// Spawns as close to [0,0] as possible on dry land.
		/// Checks radius 0 first (origin), then expands outward by 16 blocks each step.
		void computeSpawnPosition(const TerrainGenerationParams& params) {
			movement->skipDuplicateInputs = true; // server: don't re-run physics with stale inputs

			for (int radius = 0; radius <= 100; radius += 16) {
				for (int dx = -radius; dx <= radius; dx += 16) {
					for (int dz = -radius; dz <= radius; dz += 16) {
						const int surfaceY = ChunkGeneration::computeTerrainHeight(params, dx, dz);
						if (surfaceY > params.seaLevel) {
							movement->spawnPosition = glm::vec3(dx + 0.5f, surfaceY + 3, dz + 0.5f);
							movement->setPosition(movement->spawnPosition);
							movement->setYawAndPitch(0.0f, 0.0f);
							return;
						}
					}
				}
			}

			// Last resort: above the water at origin
			movement->spawnPosition = glm::vec3(0.5f, params.seaLevel + 3, 0.5f);
			movement->setPosition(movement->spawnPosition);
			movement->setYawAndPitch(0.0f, 0.0f);
		}

		std::vector<ChunkPos> rdyChunks;

		// Per-direction reliability state (seq counters + retransmit buffers).
		// One pair per player; peer is identified by addr.
		ReliabilitySender   sendRel;  // server -> this client
		ReliabilityReceiver recvRel;  // this client -> server

		bool connected; //unused
		float pingMs = -1.0f;
};

#endif