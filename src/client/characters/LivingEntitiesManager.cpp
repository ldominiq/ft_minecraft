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

		if (c->positionUpdated || c->characterBodyParts.onWalkAnimation || c->hasRenderPos)
		{
			c->characterBodyParts.character.rotation = glm::rotate(glm::mat4(1.0f), glm::radians(-c->yaw), glm::vec3(0, 1, 0));
			glm::vec3 meshPos = c->hasRenderPos ? c->renderPos : c->getPosition();
			c->characterBodyParts.character.translation = glm::translate(glm::mat4(1.0f), meshPos + c->YPositionOffset);
			// Only advance the walk animation when the entity is actually moving;
			// hasRenderPos alone (third-person camera) should not drive the animation.
			if (c->positionUpdated || c->characterBodyParts.onWalkAnimation)
				c->walkAnimation(deltaTime);
			c->characterBodyParts.character.compute(identity, projection, view, characterShader);
			c->positionUpdated = false;
		}
		else
			c->characterBodyParts.character.drawScene(characterShader, projection, view);
	}

	for (const auto &character : characters)
	{
		auto c = character.lock();
		if (!c)
			continue ;	//character expired. we removed them later
		if (!c->DoDraw())
			continue ;
		if (true)
		{
			AABB box = c->constructAABB(c->hasRenderPos ? c->renderPos : c->getPosition());
			glm::vec3 col(1.0f, 0.0f, 0.0f); // red
			hbRenderer.drawAABB(box, view, projection, col);
		}
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