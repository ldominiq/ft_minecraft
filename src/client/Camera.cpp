#include "Camera.hpp"

Camera::Camera(glm::vec3 position)
    : MouseSensitivity(0.1f) {

	//TODO position & yaw should be given by server
	glm::vec3 startingPos = glm::vec3(0,150,0);
	player = std::make_shared<ClientPlayer>(startingPos, 0, -1);
    player->updateCameraVectors();
	initWireframeCube();

	player->setDoDraw(false);
}

Camera::~Camera() {
	if (glfwGetCurrentContext()) {
		glDeleteVertexArrays(1, &wireframeVAO);
		glDeleteBuffers(1, &wireframeVBO);
		glDeleteBuffers(1, &wireframeEBO);
	} else {
		wireframeVAO = 0;
		wireframeVBO = 0;
		wireframeEBO = 0;
	}
}

glm::mat4 Camera::getViewMatrix() const
{
	glm::vec3 playerPos = player->getPosition() + glm::vec3(0, player->getEyesHeight(), 0);

	if (!thirdPersonCamera)
		return glm::lookAt(playerPos, playerPos + player->Front, player->WorldUp);

	float cameraDistance = 3.0f;  // behind the player
	float cameraHeight   = 1.5f;  // slightly above

    float yaw   = glm::radians(player->yaw);
    float pitch = glm::radians(player->pitch);

    // Direction the player is looking
    glm::vec3 forward(
        cos(pitch) * cos(yaw),
        sin(pitch),
        cos(pitch) * sin(yaw)
    );

    // Camera position BEHIND the player, opposite of forward
    glm::vec3 camPos =
        playerPos
        - forward * cameraDistance  // behind
        + glm::vec3(0, cameraHeight, 0); // slight upward offset

    return glm::lookAt(
        camPos,
        playerPos + forward * 10.0f,   // look where the player is looking
        glm::vec3(0, 1, 0)
    );
}

void Camera::predict(const Renderer &world, int32_t clientTick) //clientime broken for now
{
	if (!startPrediction)
		return ;

	float clientTime = clientTick * (1.0f / TPS);

	//make sure we start from the last state. (so lerp doesn't mess with the prediction)
	if (!predictedStates.empty())
	{
		PredictedStates lastState = predictedStates.back();
		player->setPosition(lastState.position);
		player->setVelocity(lastState.velocity);
		player->setYawAndPitch(lastState.yaw, lastState.pitch);
		player->health = lastState.health;
		// player->setSlipperinessPrev(lastState.slipperinessPrev);
	}

	//set player state to the inputs for this tick if there is.
	if (InputsMap.find(clientTick) != InputsMap.end())
	{
		player->setLastInputPacketReceived(InputsMap[clientTick]);
		player->setYawAndPitch(InputsMap[clientTick].yaw, InputsMap[clientTick].pitch);
	}

	// player->updateCameraVectors();
	player->calculateNewPosition(world);

	//construct predictions for reconcialiation and snapshots for interpolation
	predictedStates.emplace_back(PredictedStates{
		clientTick,
		player->getPosition(),
		player->getVelocity(),
		player->yaw,
		player->pitch,
		player->health,
		// player->getSlipperinessPrev()
	});

	player->snapshots.emplace_back(Snapshot{
		player->getPosition(),
		player->getVelocity(),
		clientTime
	});

	// std::cout << "tick: " << clientTick << "\n" <<
	// "pos: (" << player->getPosition().x << ", " << player->getPosition().y << ", " << player->getPosition().z << ")\n" <<
	// "vel: (" << player->getVelocity().x << ", " << player->getVelocity().y << ", " << player->getVelocity().z << ")\n" <<
	// "splitPrev: (" << player->getSlipperinessPrev() << ")\n" <<
	// "onGround: (" << player->isOnGround() << ")\n" <<
	// "fallDistance: (" << player->getAccumulatedFallDistance() << ")\n" <<
	// "jumpBoost: (" << player->getJumpBoostApplied() << ")\n";
	// std::cout << "------------------\n\n";
}

void Camera::reconcile(const PredictedStates &correction, int32_t clientTick, const Renderer &world)
{
	constexpr float kPosErrorThreshold = 0.05f; // 5 cm
	constexpr float kVelErrorThreshold = 0.05f;
	const float kPosErrorThresholdSq = kPosErrorThreshold * kPosErrorThreshold;
	const float kVelErrorThresholdSq = kVelErrorThreshold * kVelErrorThreshold;

	auto hardReconcile = [&]() {
		// restart clean from server state
		player->snapshots.clear();
		predictedStates.clear();

		player->setPosition(correction.position);
		player->setVelocity(correction.velocity);
		player->setYawAndPitch(correction.yaw, correction.pitch);
		player->health = correction.health;

		predictedStates.push_back(PredictedStates{
			correction.serverClientReconciliationTick,
			correction.position,
			correction.velocity,
			correction.yaw,
			correction.pitch,
			correction.health,
			// correction.slipperinessPrev
		});

		player->snapshots.emplace_back(Snapshot{
			correction.position,
			correction.velocity,
			correction.serverClientReconciliationTick * (1.0f / TPS)
		});

		for (int i = correction.serverClientReconciliationTick + 1; i < clientTick; i++)
			predict(world, i);
	};

	bool foundMatchingTick = false;
	for (auto it = predictedStates.begin(); it != predictedStates.end();)
	{
		if (((long)(it->serverClientReconciliationTick) - (long)(correction.serverClientReconciliationTick) < 0))
		{
			it = predictedStates.erase(it);
			continue ;
		}
		else if (it->serverClientReconciliationTick == correction.serverClientReconciliationTick)
		{
			foundMatchingTick = true;

			glm::vec3 positionDiff = it->position - correction.position;
			glm::vec3 velocityDiff = it->velocity - correction.velocity;
			float positionErrorSq = glm::dot(positionDiff, positionDiff);
			float velocityErrorSq = glm::dot(velocityDiff, velocityDiff);

			if (positionErrorSq > kPosErrorThresholdSq ||
				velocityErrorSq > kVelErrorThresholdSq ||
				it->health != correction.health ||
				it->yaw != correction.yaw ||
				it->pitch != correction.pitch)
			{
				std::cout << "format : client -> server\n";
				std::cout << "Prediction error at\n";
				std::cout << clientTick << " " << it->serverClientReconciliationTick << " " << correction.serverClientReconciliationTick << "\n";
				std::cout << "--pos---\n";
				std::cout << it->position.x << " " << it->position.y << " " << it->position.z << std::endl;
				std::cout << correction.position.x << " " << correction.position.y << " " << correction.position.z << std::endl;
				std::cout << "--vel---\n";
				std::cout << it->velocity.x << " " << it->velocity.y << " " << it->velocity.z << std::endl;
				std::cout << correction.velocity.x << " " << correction.velocity.y << " " << correction.velocity.z << std::endl;
				std::cout << "--health---\n";
				std::cout << it->health << " " << correction.health << std::endl;
				std::cout << "--yawpitch---\n";
				std::cout << it->yaw << " " << it->pitch << std::endl;
				std::cout << correction.yaw << " " << correction.pitch << std::endl;
				std::cout << "------------------\n";

				hardReconcile();
			}
			break ;
		}
		else
			it++;
	}

	if (!foundMatchingTick)
	{
		const PredictedStates *referenceState = predictedStates.empty() ? nullptr : &predictedStates.back();
		glm::vec3 positionDiff = (referenceState ? referenceState->position : player->getPosition()) - correction.position;
		glm::vec3 velocityDiff = (referenceState ? referenceState->velocity : player->getVelocity()) - correction.velocity;
		float positionErrorSq = glm::dot(positionDiff, positionDiff);
		float velocityErrorSq = glm::dot(velocityDiff, velocityDiff);

		if (positionErrorSq > kPosErrorThresholdSq ||
			velocityErrorSq > kVelErrorThresholdSq ||
			(referenceState && (referenceState->health != correction.health || referenceState->yaw != correction.yaw || referenceState->pitch != correction.pitch)))
			hardReconcile();
	}
}

void Camera::onSnapshot(NetPlayerMove &pkt, const Renderer &world, int32_t clientTick)
{
	if (!startPrediction)
		startPrediction = true;

	static int32_t lastReceivedServerClientReconciliationTick = -1;
	// if (lastReceivedServerClientReconciliationTick != -1
	// 	&& ((long)(pkt.serverClientReconciliationTick) - (long)(lastReceivedServerClientReconciliationTick) < 0))
	// 	return;
	// lastReceivedServerClientReconciliationTick = pkt.serverClientReconciliationTick;

	glm::vec3 position;
	position.x = pkt.positionX;
	position.y = pkt.positionY;
	position.z = pkt.positionZ;

	glm::vec3 velocity;
	velocity.x = pkt.velocityX;
	velocity.y = pkt.velocityY;
	velocity.z = pkt.velocityZ;

	PredictedStates correction = PredictedStates{
		pkt.serverClientReconciliationTick,
		position,
		velocity,
		pkt.yaw,
		pkt.pitch,
		pkt.health,
		// pkt.slipperinessPrev
	};

	reconcile(correction, clientTick, world);

	// Clean up old inputs. 200 is an arbitrary number and is just used to avoid iterating on each loop on a map.
	if (InputsMap.size() > 200)
	{
		for (auto it = InputsMap.begin(); it != InputsMap.end();)
		{
			if (((long)(it->first) - (long)(correction.serverClientReconciliationTick) < 0))
				it = InputsMap.erase(it);
			else
				it++;
		}
	}
}

void Camera::processMouseMovement(float xoffset, float yoffset) {
    xoffset *= MouseSensitivity;
    yoffset *= MouseSensitivity;

    player->yaw   += xoffset;
    player->pitch += yoffset;

    if (player->pitch > 89.0f)  player->pitch = 89.0f;
    if (player->pitch < -89.0f) player->pitch = -89.0f;

	player->updateCameraVectors();
}

void Camera::initWireframeCube() {

    glm::vec3 vertices[8] = {
        {0, 0, 0},
        {1, 0, 0},
        {1, 1, 0},
        {0, 1, 0},
        {0, 0, 1},
        {1, 0, 1},
        {1, 1, 1},
        {0, 1, 1},
    };

    GLuint indices[] = {
        0,1, 1,2, 2,3, 3,0, // bottom face
        4,5, 5,6, 6,7, 7,4, // top face
        0,4, 1,5, 2,6, 3,7  // vertical lines
    };

    glGenVertexArrays(1, &wireframeVAO);
    glGenBuffers(1, &wireframeVBO);
    glGenBuffers(1, &wireframeEBO);

    glBindVertexArray(wireframeVAO);

    glBindBuffer(GL_ARRAY_BUFFER, wireframeVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, wireframeEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Vertex layout: 3 floats per vertex
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0); // Unbind VAO

	blockWireframeShader = std::make_unique<Shader>("shaders/simpleWireframe.vert", "shaders/simpleWireframe.frag");
}

void Camera::drawWireframeSelectedBlockFace(std::shared_ptr<Renderer> &Renderer, glm::mat4 &view, glm::mat4 &projection) {

	glm::ivec3 blockPos{};
	glm::ivec3 faceNormal{};
	LivingEntity* livingEntity = nullptr;

	if (Renderer->getTarget(*player, blockPos, faceNormal, livingEntity) != TargetType::Block)
		return ;

	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(blockPos));

	blockWireframeShader->use();
	blockWireframeShader->setMat4("model", model);
	blockWireframeShader->setMat4("view", view);
	blockWireframeShader->setMat4("projection", projection);
	blockWireframeShader->setVec3("color", glm::vec3(1.0f, 0.0f, 1.0f));

    glBindVertexArray(wireframeVAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}
