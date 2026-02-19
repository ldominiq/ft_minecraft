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

void LivingEntitiesManager::add(std::weak_ptr<IClientEntity> character)
{
	characters.push_back(character);
}

void LivingEntitiesManager::draw(const glm::mat4 &projection, const glm::mat4 &view, const float deltaTime)
{
	characterShader.use();
	auto identity = glm::mat4(1.0f);

	for (const auto &character : characters)
	{
		auto c = character.lock();
		if (!c)
			continue ;	//character expired. we removed them later

		if (!c->DoDraw())
			continue ;

		if (c->positionUpdated || c->characterBodyParts.onWalkAnimation)
		{
			c->characterBodyParts.character.rotation = glm::rotate(glm::mat4(1.0f), glm::radians(-c->yaw), glm::vec3(0, 1, 0));
			c->characterBodyParts.character.translation = glm::translate(glm::mat4(1.0f), c->getPosition() + c->YPositionOffset);
			c->characterBodyParts.character.compute(identity, projection, view, characterShader);
			c->walkAnimation(deltaTime);
			c->positionUpdated = false;
		}
		else
			c->characterBodyParts.character.drawScene(characterShader, projection, view);
	}

	//remove expired characters.
	characters.erase(
		std::remove_if(characters.begin(), characters.end(),
			[](const std::weak_ptr<IClientEntity>& w){
				return w.expired();
			}),
		characters.end()
	);
}