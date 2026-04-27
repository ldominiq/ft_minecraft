#include "LivingEntitiesManager.hpp"
#include "ClientCreeper.hpp"
#include <cmath>

LivingEntitiesManager::LivingEntitiesManager() : characterShader("shaders/characterCube.vert", "shaders/characterCube.frag")
{
	//builds the meshes for the cube (body parts)
	createCube();

	// Load entity skins. Missing files fall through — manager::get returns 0
	// and we render with per-limb colors as a fallback.
	skinManager.load("player",  "assets/skins/steve.png");
	skinManager.load("creeper", "assets/skins/creeper.png");
	skinManager.load("zombie", "assets/skins/zombie.png");
}

LivingEntitiesManager::~LivingEntitiesManager()
{
	destroyCube();
}

void LivingEntitiesManager::add(std::weak_ptr<IClientEntity> character)
{
	characters.push_back(character);
}

void LivingEntitiesManager::draw(const glm::mat4 &projection, const glm::mat4 &view,
								 const glm::dvec3& eyePos, const float deltaTime)
{
  static glm::dvec3 prevEyePos(0.0);
	static bool hasPrevEyePos = false;
	const glm::dvec3 eyeDelta = eyePos - prevEyePos;
	const bool cameraMoved = !hasPrevEyePos || glm::dot(eyeDelta, eyeDelta) > 1e-12;
	prevEyePos = eyePos;
	hasPrevEyePos = true;

	characterShader.use();
	characterShader.setInt("uSkin", 0);
    characterShader.setMat4("uProjection", projection);
	glm::mat4 viewRot = view;
	viewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	characterShader.setMat4("uViewRot", viewRot);
	auto identity = glm::mat4(1.0f);

	for (const auto &character : characters)
	{
		auto c = character.lock();
		if (!c)
			continue ;	//character expired. we removed them later

		if (!c->DoDraw())
			continue ;

		// Bind this entity's skin (if any) before issuing its draw calls.
		GLuint skin = skinManager.get(c->skinName());
		if (skin)
		{
			glActiveTexture(GL_TEXTURE0);
			glBindTexture(GL_TEXTURE_2D, skin);
			characterShader.setBool("uUseTexture", true);
		}
		else
		{
			characterShader.setBool("uUseTexture", false);
		}

		const bool swinging = c->characterBodyParts.onArmSwingAnimation;
		const bool dying = c->characterBodyParts.dying;

		// Advance creeper inflate/deflate every frame
		auto cc = std::dynamic_pointer_cast<ClientCreeper>(c);
		if (cc && !dying)
			cc->tickFuseAnimation(deltaTime);

        const bool creeperAnimating = cc && (cc->clientPrimed || cc->inflation > 0.0f);

		const bool isLocalPlayer = (c->getID() == static_cast<entityID>(-1));
		if (!isLocalPlayer) {
			const glm::dvec3 targetPosD = c->getPositionD();
			if (!c->hasRenderPos) {
				c->renderPos = targetPosD;
				c->hasRenderPos = true;
			} else {
				const double alpha = 1.0 - std::exp(-static_cast<double>(deltaTime) * 22.0);
				c->renderPos = glm::mix(c->renderPos, targetPosD, alpha);
			}
		}

		if (dying || c->positionUpdated || c->rotationUpdated || c->characterBodyParts.onWalkAnimation || swinging || c->hasRenderPos || creeperAnimating || cameraMoved)
		{
			c->characterBodyParts.character.rotation = glm::rotate(glm::mat4(1.0f), glm::radians(-c->yaw), glm::vec3(0, 1, 0));
          glm::dvec3 meshPosD = c->hasRenderPos ? c->renderPos : c->getPositionD();
			glm::dvec3 meshPosRelD = meshPosD - eyePos;
			c->characterBodyParts.character.translation = glm::translate(
				glm::mat4(1.0f), glm::vec3(meshPosRelD + glm::dvec3(c->YPositionOffset)));
			if (dying)
			{
				c->deathAnimation(deltaTime);
				if (c->characterBodyParts.dyingDone)
					c->removed = true;
			}
			else
			{
				// Only advance the walk animation when the entity is actually moving;
				// hasRenderPos alone (third-person camera) should not drive the animation.
				if ((c->positionUpdated && c->hasHorizontalInput) || c->characterBodyParts.onWalkAnimation)
					c->walkAnimation(deltaTime);
				c->applyHeadPitch(c->pitch);
				if (swinging)
					c->swingArmAnimation(deltaTime);
			}
           c->characterBodyParts.character.compute(identity, projection, viewRot, characterShader);
			c->positionUpdated = false;
			c->rotationUpdated = false;
		}
		else
           c->characterBodyParts.character.drawScene(characterShader, projection, viewRot);
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
            const glm::dvec3 posD = c->hasRenderPos ? c->renderPos : c->getPositionD();
			const double halfW = static_cast<double>(c->getEntityWidth()) * 0.5;
			const double h = static_cast<double>(c->getEntityHeight());
			const glm::dvec3 minRelD(posD.x - eyePos.x - halfW,
									 posD.y - eyePos.y,
									 posD.z - eyePos.z - halfW);
			const glm::dvec3 maxRelD(posD.x - eyePos.x + halfW,
									 posD.y - eyePos.y + h,
									 posD.z - eyePos.z + halfW);
           AABB box{glm::vec3(minRelD), glm::vec3(maxRelD)};
			glm::vec3 col(1.0f, 0.0f, 0.0f); // red
            hbRenderer.drawAABB(box, view, projection, glm::dvec3(0.0), col);
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
