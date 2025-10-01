
#include "ItemEntity.hpp"

ItemEntity::ItemEntity(glm::vec3 position, float yaw, ItemID ID):	Entity(position)
{
	item = ID;
	this->yaw = yaw;
}

ItemEntity::ItemEntity(glm::vec3 position, float yaw, ItemID ID, uint32_t entityID): Entity(position, entityID) 
{
	item = ID;
	this->yaw = yaw;
}

ItemEntity::~ItemEntity() {}

glm::vec3 ItemEntity::getDesiredMove()
{
	return glm::vec3(0,0,0);
}