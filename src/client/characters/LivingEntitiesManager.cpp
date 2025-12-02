#include "LivingEntitiesManager.hpp"

LivingEntitiesManager::LivingEntitiesManager() : characterShader("shaders/characterCube.vert", "shaders/characterCube.frag")
{
	//builds the meshes for the cube (body parts)
	createCube();
}

LivingEntitiesManager::~LivingEntitiesManager()
{
	destroyCube();
}

void LivingEntitiesManager::add(std::shared_ptr<Character> character)
{
	characters.push_back(character);
}

void LivingEntitiesManager::draw(const glm::mat4 &projection, const glm::mat4 &view, const float deltatima)
{
	characterShader.use();
	auto identity = glm::mat4(1.0f);
	for (const auto &c : characters)
	{
		if (!c->DoDraw()) return ;
		if (c->positionUpdated || c->c.onWalkAnimation)
		{
			c->c.character.rotation = glm::rotate(glm::mat4(1.0f), glm::radians(-c->yaw), glm::vec3(0, 1, 0));
			c->c.character.translation = glm::translate(glm::mat4(1.0f), c->getPosition());
			c->c.character.compute(identity, projection, view, characterShader);
			c->walkAnimation(deltatima);
			c->positionUpdated = false;
		}
		else
			c->c.character.drawScene(characterShader, projection, view);
	}
}