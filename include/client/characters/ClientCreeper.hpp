
#ifndef CLIENT_CREEPER_HPP
#define CLIENT_CREEPER_HPP

#include "IClientEntity.hpp"
#include "Creeper.hpp"

class ClientCreeper : public Creeper, public IClientEntity
{
		public :

		ClientCreeper(const glm::vec3 &position, float yaw, entityID ID);
		~ClientCreeper() = default;
};

#endif