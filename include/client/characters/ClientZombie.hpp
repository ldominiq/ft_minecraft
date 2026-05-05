
#ifndef CLIENT_ZOMBIE_HPP
#define CLIENT_ZOMBIE_HPP

#include "IClientEntity.hpp"
#include "Zombie.hpp"

class ClientZombie : public Zombie, public IClientEntity
{
		public :

		ClientZombie(const glm::vec3 &position, float yaw, entityID ID);
		~ClientZombie() = default;

		void walkAnimation(float deltaTime) override;
		
		std::string skinName() const override { return "zombie"; }
};

#endif
