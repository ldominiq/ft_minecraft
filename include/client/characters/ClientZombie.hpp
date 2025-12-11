#ifndef CLIENT_ZOMBIE_HPP
#define CLIENT_ZOMBIE_HPP

#include "IClientEntity.hpp"
#include "Zombie.hpp"

class ClientZombie : public Zombie, public IClientEntity
{
	void createCharacterAt(const glm::vec3 &pos, float characterScale) override;

		public :

		ClientZombie(const glm::vec3 &position, float yaw, entityID ID);
		~ClientZombie() = default;
		
		void walkAnimation(float deltaTime) override;
		void jumpAnimation(float currentFrame) override;
};

#endif