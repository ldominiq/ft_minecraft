#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>
#include <memory>
#include <ranges>
#include <deque>
#include <optional>
#include <cstdint>
#include <algorithm>

#include "Protocol.hpp"
#include "Renderer.hpp"
#include "GLFW/glfw3.h"
#include "ClientPlayer.hpp"

struct PredictedStates
{
	int32_t serverClientReconciliationTick;
	glm::vec3 position;
	glm::vec3 velocity;
	float yaw;
	float pitch;
	float health;
	float slipperinessPrev;
	float accumulatedFallDistance;
	bool onGround;
	bool jumpBoostApplied;
};

struct ReconcileDebugStats {
	int32_t lastClientTick = -1;
	int32_t lastAckTick = -1;
    int32_t lastEffectiveAckTick = -1;
	float lastPosErr = 0.0f;
	float lastVelErr = 0.0f;
	float lastHorizontalErr = 0.0f;
	float lastVerticalErr = 0.0f;
	float lastErrAtAckTick = 0.0f;
	float lastErrAtAckMinusOneTick = 0.0f;
	uint64_t totalCorrections = 0;
	uint64_t appliedCorrections = 0;
	uint64_t ignoredCorrections = 0;
	uint64_t suspectedOffByOneCorrections = 0;
};

class Camera {

	GLuint wireframeVAO, wireframeVBO, wireframeEBO;

	void initWireframeCube();
	std::unique_ptr<Shader> blockWireframeShader = nullptr;

	bool startPrediction = false;

	std::shared_ptr<ClientPlayer> player;
	bool thirdPersonCamera = false;
	// std::unordered_map<int32_t, PredictedStates> predictedStates;
	static constexpr int32_t kMaxPredictedStates = 120; // 6 s at 20 TPS
	std::deque<PredictedStates> predictedStates;

	void reconcile(const PredictedStates &correction, int32_t clientTick, const Renderer &world);
	std::map<int32_t, NetPlayerInputs> InputsMap;
	std::optional<PredictedStates> pendingCorrection;
	int32_t pendingCorrectionTick = -1;
	int32_t lastAppliedServerClientReconciliationTick = -1;

	float reconcilePosErrorThreshold = 0.05f;
	float reconcileVelErrorThreshold = 0.05f;
	bool reconcileLogEnabled = true;
   bool reconcileAutoPhaseAdjust = false;
	float renderTickAlpha = 0.0f;
	glm::vec3 renderPrevPosition = glm::vec3(0.0f);
	glm::vec3 renderCurrPosition = glm::vec3(0.0f);
	bool renderPositionInitialized = false;

	ReconcileDebugStats reconcileDebugStats;

public:

    float MouseSensitivity{};

    explicit Camera(glm::vec3 position);
	~Camera();

    glm::mat4 getViewMatrix() const;
    void processMouseMovement(float xoffset, float yoffset);
	void onSnapshot(NetPlayerMove &pkt, const Renderer &world, int32_t clientTick);
    void flushPendingSnapshot(const Renderer &world, int32_t clientTick);
	Snapshot getLatestSnapshot() { return getPlayer()->snapshots.empty() ? Snapshot{} : getPlayer()->snapshots.back(); }

	void drawWireframeSelectedBlockFace(std::shared_ptr<Renderer> &Renderer, glm::mat4 &view, glm::mat4 &projection);

	const inline bool isThirdPersonCameraActive() const {return thirdPersonCamera;}
	const inline void toggleThirdPersonCamera() {thirdPersonCamera = !thirdPersonCamera; player->setDoDraw(thirdPersonCamera);}
	inline std::shared_ptr<ClientPlayer> getPlayer() {return player;};

	void predict(const Renderer &world, int32_t clientTick);
	inline void queueInput(const NetPlayerInputs &inputs, int32_t clientTick) { InputsMap[clientTick] = inputs; }

	inline size_t getPendingInputCount() const { return InputsMap.size(); }
	inline size_t getPredictedStateCount() const { return predictedStates.size(); }
	inline int32_t getPendingCorrectionTick() const { return pendingCorrectionTick; }
	inline int32_t getLastAppliedAckTick() const { return lastAppliedServerClientReconciliationTick; }
	inline const ReconcileDebugStats &getReconcileDebugStats() const { return reconcileDebugStats; }

	inline float getReconcilePosErrorThreshold() const { return reconcilePosErrorThreshold; }
	inline float getReconcileVelErrorThreshold() const { return reconcileVelErrorThreshold; }
	inline bool isReconcileLogEnabled() const { return reconcileLogEnabled; }
	inline bool isReconcileAutoPhaseAdjustEnabled() const { return reconcileAutoPhaseAdjust; }

	inline void setReconcilePosErrorThreshold(float value) { reconcilePosErrorThreshold = value; }
	inline void setReconcileVelErrorThreshold(float value) { reconcileVelErrorThreshold = value; }
	inline void setReconcileLogEnabled(bool value) { reconcileLogEnabled = value; }
	inline void setReconcileAutoPhaseAdjustEnabled(bool value) { reconcileAutoPhaseAdjust = value; }
	inline void setRenderTickAlpha(float value) { renderTickAlpha = std::clamp(value, 0.0f, 1.0f); }

	glm::vec3 visualOffset = glm::vec3(0.0f);
	void updateSmoothing(float deltaTime);
};

#endif // CAMERA_HPP