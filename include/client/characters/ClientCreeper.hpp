
#ifndef CLIENT_CREEPER_HPP
#define CLIENT_CREEPER_HPP

#include "IClientEntity.hpp"
#include "Creeper.hpp"

class ClientCreeper : public Creeper, public IClientEntity
{
	void createCharacterAt(const glm::vec3 &pos) override;

		public :

		ClientCreeper(const glm::vec3 &position, float yaw, entityID ID);
		~ClientCreeper() = default;
		
		// void walkAnimation(float deltaTime) override;
		// void jumpAnimation(float currentFrame) override;
};

#endif