#include "Camera.hpp"
#include <cmath>
#include <limits>

Camera::Camera(glm::vec3 position)
    : MouseSensitivity(0.1f) {

	//TODO position & yaw should be given by server
	glm::vec3 startingPos = glm::vec3(0,150,0);
	player = std::make_shared<ClientPlayer>(startingPos, 0.0f, static_cast<entityID>(-1));
	renderPrevPosition = startingPos;
	renderCurrPosition = startingPos;
	renderPositionInitialized = true;
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
  // Interpolation done in double so the per-frame eye position keeps
  // sub-cm precision even at very large world coordinates. The view
  // matrix returned is still mat4 (translation column quantized to
  // float), but the chunk renderer reads getEyePosD() separately to
  // build chunkRel without going through this float matrix.
  glm::dvec3 interpolatedPosD = player->getPositionD();
	if (renderPositionInitialized)
		interpolatedPosD = glm::mix(renderPrevPosition, renderCurrPosition, static_cast<double>(renderTickAlpha));
    glm::dvec3 playerPosD = interpolatedPosD
		+ glm::dvec3(0.0, static_cast<double>(player->getEyesHeight()), 0.0)
		+ glm::dvec3(visualOffset);

	if (!thirdPersonCamera) {
		const glm::dvec3 centerD = playerPosD + glm::dvec3(player->Front);
		return glm::mat4(glm::lookAt(playerPosD, centerD, glm::dvec3(player->WorldUp)));
	}

	float cameraDistance = 3.0f;  // behind the player
	float cameraHeight   = 1.5f;  // slightly above

	float yaw   = glm::radians(player->yaw);
	float pitch = glm::radians(player->pitch);

	// Direction the player is looking
  glm::dvec3 forward(
		cos(pitch) * cos(yaw),
		sin(pitch),
		cos(pitch) * sin(yaw)
	);

	// Camera position BEHIND the player, opposite of forward
  glm::dvec3 camPosD =
		playerPosD
       - forward * static_cast<double>(cameraDistance)  // behind
		+ glm::dvec3(0.0, static_cast<double>(cameraHeight), 0.0); // slight upward offset

	const glm::dvec3 targetD = playerPosD + forward * 10.0;

 return glm::mat4(glm::lookAt(
		camPosD,
		targetD,
		glm::dvec3(0.0, 1.0, 0.0)
	));
}

glm::dvec3 Camera::getEyePosD() const
{
	glm::dvec3 interpolatedPosD = player->getPositionD();
	if (renderPositionInitialized)
		interpolatedPosD = glm::mix(renderPrevPosition, renderCurrPosition, static_cast<double>(renderTickAlpha));
	glm::dvec3 playerPosD = interpolatedPosD
		+ glm::dvec3(0.0, static_cast<double>(player->getEyesHeight()), 0.0)
		+ glm::dvec3(visualOffset);

	if (!thirdPersonCamera)
		return playerPosD;

	// Third person: replicate getViewMatrix's third-person eye math in double.
	const float cameraDistance = 3.0f;
	const float cameraHeight   = 1.5f;
	const float yawRad   = glm::radians(player->yaw);
	const float pitchRad = glm::radians(player->pitch);
	const glm::dvec3 forward(
		std::cos(pitchRad) * std::cos(yawRad),
		std::sin(pitchRad),
		std::cos(pitchRad) * std::sin(yawRad)
	);
	return playerPosD - forward * static_cast<double>(cameraDistance)
		+ glm::dvec3(0.0, static_cast<double>(cameraHeight), 0.0);
}

void Camera::updateSmoothing(float deltaTime) {
   const float len2 = glm::dot(visualOffset, visualOffset);
	if (len2 > 1e-6f) {
		constexpr float decayRate = 12.0f;
		const float decay = std::exp(-decayRate * std::max(deltaTime, 0.0f));
		visualOffset *= decay;
		if (glm::dot(visualOffset, visualOffset) < 1e-6f)
			visualOffset = glm::vec3(0.0f);
	} else {
		visualOffset = glm::vec3(0.0f);
	}
}


void Camera::predict(const Renderer &world, int32_t clientTick) //clientime broken for now
{
	if (!startPrediction)
		return ;

	// freeze local simulation while dead
	if (player->health <= 0) {
		player->setVelocity(glm::vec3(0.0f));
		renderPrevPosition = player->getPositionD();
		renderCurrPosition = player->getPositionD();
		renderPositionInitialized = true;
		return;
	}

	float clientTime = clientTick * (1.0f / TPS);

	//make sure we start from the last state. (so lerp doesn't mess with the prediction)
	if (!predictedStates.empty())
	{
		PredictedStates lastState = predictedStates.back();
		player->setPosition(lastState.position);
		player->setVelocity(lastState.velocity);
		player->setYawAndPitch(lastState.yaw, lastState.pitch);
		player->health = lastState.health;
     player->setSlipperinessPrev(lastState.slipperinessPrev);
		player->accumulatedFallDistance = lastState.accumulatedFallDistance;
		player->setOnGround(lastState.onGround);
		player->setJumpBoostApplied(lastState.jumpBoostApplied);
	}

	//set player state to the inputs for this tick if there is.
	if (InputsMap.find(clientTick) != InputsMap.end())
	{
		player->setLastInputPacketReceived(InputsMap[clientTick]);
		player->setYawAndPitch(InputsMap[clientTick].yaw, InputsMap[clientTick].pitch);
	}

	// player->updateCameraVectors();
	player->calculateNewPosition(world);

	if (!renderPositionInitialized) {
		renderPrevPosition = player->getPositionD();
		renderCurrPosition = player->getPositionD();
		renderPositionInitialized = true;
	} else {
		renderPrevPosition = renderCurrPosition;
		renderCurrPosition = player->getPositionD();
	}

	//construct predictions for reconcialiation and snapshots for interpolation
	predictedStates.emplace_back(PredictedStates{
		clientTick,
		player->getPositionD(),
		player->getVelocity(),
		player->yaw,
		player->pitch,
		player->health,
        player->getSlipperinessPrev(),
		player->getAccumulatedFallDistance(),
		player->isOnGround(),
		player->getJumpBoostApplied(),
	});

	player->snapshots.emplace_back(Snapshot{
		player->getPosition(),
		player->getVelocity(),
		clientTime
	});

	if (static_cast<int32_t>(predictedStates.size()) > kMaxPredictedStates)
		predictedStates.pop_front();

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
  reconcileDebugStats.totalCorrections++;
	reconcileDebugStats.lastClientTick = clientTick;
	reconcileDebugStats.lastAckTick = correction.serverClientReconciliationTick;
	reconcileDebugStats.lastEffectiveAckTick = correction.serverClientReconciliationTick;

	PredictedStates effectiveCorrection = correction;

	const float localYaw = player->yaw;
	const float localPitch = player->pitch;

 auto it = std::find_if(predictedStates.begin(), predictedStates.end(), [&](const PredictedStates& st) {
		return st.serverClientReconciliationTick == correction.serverClientReconciliationTick;
	});

	if (reconcileAutoPhaseAdjust && !predictedStates.empty()) {
		auto scoreForTick = [&](int32_t tickCandidate) -> float {
			auto candIt = std::find_if(predictedStates.begin(), predictedStates.end(), [&](const PredictedStates& st) {
				return st.serverClientReconciliationTick == tickCandidate;
			});
			if (candIt == predictedStates.end())
				return std::numeric_limits<float>::max();

			glm::dvec3 posDiff = candIt->position - correction.position;
			glm::vec3 velDiff = candIt->velocity - correction.velocity;
			return static_cast<float>(glm::length(posDiff)) + glm::length(velDiff) * 0.25f;
		};

		const int32_t ack = correction.serverClientReconciliationTick;
		const float scoreMinusOne = scoreForTick(ack - 1);
		const float scoreAck = scoreForTick(ack);
		const float scorePlusOne = scoreForTick(ack + 1);

		int32_t bestTick = ack;
		float bestScore = scoreAck;
		if (scoreMinusOne < bestScore) {
			bestScore = scoreMinusOne;
			bestTick = ack - 1;
		}
		if (scorePlusOne < bestScore) {
			bestScore = scorePlusOne;
			bestTick = ack + 1;
		}

		effectiveCorrection.serverClientReconciliationTick = bestTick;
		reconcileDebugStats.lastEffectiveAckTick = bestTick;
	}

	it = std::find_if(predictedStates.begin(), predictedStates.end(), [&](const PredictedStates& st) {
		return st.serverClientReconciliationTick == effectiveCorrection.serverClientReconciliationTick;
	});

	if (it != predictedStates.end()) {
     glm::dvec3 posDiff = it->position - effectiveCorrection.position;
		glm::vec3 velDiff = it->velocity - effectiveCorrection.velocity;

		float hPosErr = static_cast<float>(std::sqrt(posDiff.x * posDiff.x + posDiff.z * posDiff.z));
		float vPosErr = static_cast<float>(std::abs(posDiff.y));
		float vErr = glm::length(velDiff);
		reconcileDebugStats.lastErrAtAckTick = static_cast<float>(glm::length(posDiff));

		if (it != predictedStates.begin()) {
			auto prevIt = std::prev(it);
         glm::dvec3 prevPosDiff = prevIt->position - effectiveCorrection.position;
			reconcileDebugStats.lastErrAtAckMinusOneTick = static_cast<float>(glm::length(prevPosDiff));
			if (reconcileDebugStats.lastErrAtAckMinusOneTick + 0.01f < reconcileDebugStats.lastErrAtAckTick)
				reconcileDebugStats.suspectedOffByOneCorrections++;
		} else {
			reconcileDebugStats.lastErrAtAckMinusOneTick = 0.0f;
		}

		// If disagreement is below threshold, prune history and trust client simulation
       if (hPosErr < reconcilePosErrorThreshold && vPosErr < reconcilePosErrorThreshold && vErr < reconcileVelErrorThreshold) {
			reconcileDebugStats.ignoredCorrections++;
			predictedStates.erase(predictedStates.begin(), it);

            float correctionTime = effectiveCorrection.serverClientReconciliationTick * (1.0f / TPS);
			auto snapIt = std::find_if(player->snapshots.begin(), player->snapshots.end(), [&](const Snapshot& sn) {
				return sn.time >= correctionTime;   
			});
			if (snapIt != player->snapshots.begin()) {
				player->snapshots.erase(player->snapshots.begin(), snapIt);
			}
			return;
		}
	}

	// Guard: stale correction older than oldest history entry — already processed
	if (!predictedStates.empty() &&
     effectiveCorrection.serverClientReconciliationTick < predictedStates.front().serverClientReconciliationTick)
	{
		return;
	}

	const PredictedStates oldCurrentState = predictedStates.empty()
		? PredictedStates{
			clientTick,
			player->getPositionD(),
			player->getVelocity(),
			player->yaw,
			player->pitch,
			player->health,
		   player->getSlipperinessPrev(),
			player->getAccumulatedFallDistance(),
			player->isOnGround(),
			player->getJumpBoostApplied(),
		}
		: predictedStates.back();

	player->snapshots.clear();
	predictedStates.clear();

   player->setPosition(effectiveCorrection.position);
	player->setVelocity(effectiveCorrection.velocity);
	player->health = effectiveCorrection.health;
	player->setSlipperinessPrev(effectiveCorrection.slipperinessPrev);
	player->accumulatedFallDistance = effectiveCorrection.accumulatedFallDistance;
	player->setOnGround(effectiveCorrection.onGround);
	player->setJumpBoostApplied(effectiveCorrection.jumpBoostApplied);

	predictedStates.push_back(PredictedStates{
      effectiveCorrection.serverClientReconciliationTick,
		effectiveCorrection.position,
		effectiveCorrection.velocity,
		effectiveCorrection.yaw,
		effectiveCorrection.pitch,
		effectiveCorrection.health,
	 effectiveCorrection.slipperinessPrev,
		effectiveCorrection.accumulatedFallDistance,
		effectiveCorrection.onGround,
		effectiveCorrection.jumpBoostApplied,
	});

	player->snapshots.emplace_back(Snapshot{
		effectiveCorrection.position,
		effectiveCorrection.velocity,
		effectiveCorrection.serverClientReconciliationTick * (1.0f / TPS)
	});

    for (int i = effectiveCorrection.serverClientReconciliationTick + 1; i < clientTick; i++)
		predict(world, i);

	const PredictedStates reconciledCurrentState = predictedStates.empty()
		? PredictedStates{
			clientTick,
			player->getPositionD(),
			player->getVelocity(),
			player->yaw,
			player->pitch,
			player->health,
		   player->getSlipperinessPrev(),
			player->getAccumulatedFallDistance(),
			player->isOnGround(),
			player->getJumpBoostApplied(),
		}
		: predictedStates.back();

	glm::dvec3 positionDiff = reconciledCurrentState.position - oldCurrentState.position;
	glm::vec3 velocityDiff = reconciledCurrentState.velocity - oldCurrentState.velocity;
	const float horizontalPosErr = static_cast<float>(std::sqrt(positionDiff.x * positionDiff.x + positionDiff.z * positionDiff.z));
	const float verticalPosErr = static_cast<float>(std::abs(positionDiff.y));
	const float velocityErr = std::sqrt(glm::dot(velocityDiff, velocityDiff));
	reconcileDebugStats.lastPosErr = static_cast<float>(std::sqrt(glm::dot(positionDiff, positionDiff)));
	reconcileDebugStats.lastVelErr = velocityErr;
	reconcileDebugStats.lastHorizontalErr = horizontalPosErr;
	reconcileDebugStats.lastVerticalErr = verticalPosErr;
	reconcileDebugStats.appliedCorrections++;

    if (horizontalPosErr > reconcilePosErrorThreshold || verticalPosErr > reconcilePosErrorThreshold || velocityErr > reconcileVelErrorThreshold)
	{
        // Smooth visual popping by setting a bounded camera offset that decays over time.
		// Computed in double then downcast — the diff itself is small (sub-block).
		glm::dvec3 desiredVisualOffsetD = oldCurrentState.position - reconciledCurrentState.position;
		constexpr float kMaxVisualOffset = 0.35f;
		const float desiredLen = static_cast<float>(glm::length(desiredVisualOffsetD));
		if (desiredLen > kMaxVisualOffset && desiredLen > 0.0f)
			desiredVisualOffsetD = (desiredVisualOffsetD / static_cast<double>(desiredLen)) * static_cast<double>(kMaxVisualOffset);
		visualOffset = glm::vec3(desiredVisualOffsetD);

        if (reconcileLogEnabled) {
			std::cout << "[RECONCILE] clientTick=" << clientTick
				<< " ackTick=" << correction.serverClientReconciliationTick
				<< " effectiveAckTick=" << effectiveCorrection.serverClientReconciliationTick
				<< " posErr=" << std::sqrt(glm::dot(positionDiff, positionDiff))
				<< " velErr=" << velocityErr
				<< " horizontalErr=" << horizontalPosErr
				<< " verticalErr=" << verticalPosErr
				<< " oldPos=(" << oldCurrentState.position.x << ", " << oldCurrentState.position.y << ", " << oldCurrentState.position.z << ")"
				<< " newPos=(" << reconciledCurrentState.position.x << ", " << reconciledCurrentState.position.y << ", " << reconciledCurrentState.position.z << ")\n";

			if (reconcileDebugStats.lastErrAtAckMinusOneTick + 0.01f < reconcileDebugStats.lastErrAtAckTick) {
				std::cout << "[RECONCILE_HINT] snapshot may be phase-shifted by ~1 tick: err(ackTick)="
					<< reconcileDebugStats.lastErrAtAckTick
					<< " err(ackTick-1)="
					<< reconcileDebugStats.lastErrAtAckMinusOneTick
					<< "\n";
			}
		}
	}

	player->setYawAndPitch(localYaw, localPitch);
	player->updateCameraVectors();
   if (!predictedStates.empty())
	{
		predictedStates.back().yaw = localYaw;
		predictedStates.back().pitch = localPitch;
	}

	renderPrevPosition = player->getPositionD();
	renderCurrPosition = player->getPositionD();
	renderPositionInitialized = true;
}

void Camera::onSnapshot(NetPlayerMove &pkt)
{
	if (!startPrediction)
		startPrediction = true;

	if (lastAppliedServerClientReconciliationTick != -1 &&
		((long)(pkt.serverClientReconciliationTick) - (long)(lastAppliedServerClientReconciliationTick) <= 0))
		return;

	glm::dvec3 position;
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
     pkt.slipperinessPrev,
		pkt.accumulatedFallDistance,
		pkt.onGround != 0,
		pkt.jumpBoostApplied != 0,
	};

	if (!pendingCorrection.has_value() ||
		((long)(correction.serverClientReconciliationTick) - (long)(pendingCorrectionTick) > 0))
	{
		pendingCorrection = correction;
		pendingCorrectionTick = correction.serverClientReconciliationTick;
	}

}

void Camera::flushPendingSnapshot(const Renderer &world, int32_t clientTick)
{
	if (!pendingCorrection.has_value())
		return;

	reconcile(*pendingCorrection, clientTick, world);
	lastAppliedServerClientReconciliationTick = pendingCorrection->serverClientReconciliationTick;

	const auto correctionTick = pendingCorrection->serverClientReconciliationTick;
	pendingCorrection.reset();
	pendingCorrectionTick = -1;

	// Clean up inputs older than the correction tick.
	for (auto it = InputsMap.begin(); it != InputsMap.end();)
	{
		if (((long)(it->first) - (long)(correctionTick) < 0))
			it = InputsMap.erase(it);
		else
			it++;
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

 const glm::dvec3 eyePosD = getEyePosD();
	const glm::dvec3 blockPosRelD = glm::dvec3(blockPos) - eyePosD;
	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(blockPosRelD));

	glm::mat4 viewRot = view;
	viewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	blockWireframeShader->use();
	blockWireframeShader->setMat4("model", model);
    blockWireframeShader->setMat4("view", viewRot);
	blockWireframeShader->setMat4("projection", projection);
	blockWireframeShader->setVec3("color", glm::vec3(1.0f, 0.0f, 1.0f));

    glBindVertexArray(wireframeVAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}
