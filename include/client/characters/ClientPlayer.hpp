
#ifndef CLIENT_PLAYER_HPP
#define CLIENT_PLAYER_HPP

#include "IClientEntity.hpp"
#include "PlayerMovement.hpp"

class ClientPlayer : public PlayerMovement, public IClientEntity
{
	// void createCharacterAt(const glm::vec3 &pos) override;

		public :

		ClientPlayer(const glm::vec3 &position, float yaw, entityID ID);
		~ClientPlayer() = default;
		
		// void walkAnimation(float deltaTime) override;
		// void jumpAnimation(float currentFrame) override;
};

#endif