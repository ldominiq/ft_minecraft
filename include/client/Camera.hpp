#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>
#include <memory>
#include <ranges>
#include <deque>

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

	// float slipperinessPrev;
	// bool onGround;
};

class Camera {

	GLuint wireframeVAO, wireframeVBO, wireframeEBO;

	void initWireframeCube();
	std::unique_ptr<Shader> blockWireframeShader = nullptr;

	bool startPrediction = false;

	std::shared_ptr<ClientPlayer> player;
	bool thirdPersonCamera = false;
	// std::unordered_map<int32_t, PredictedStates> predictedStates;
	std::vector<PredictedStates> predictedStates;

	void reconcile(const PredictedStates &correction, int32_t clientTick, const Renderer &world);
	std::map<int32_t, NetPlayerInputs> InputsMap;

public:

    float MouseSensitivity{};

    explicit Camera(glm::vec3 position);
	~Camera();

    glm::mat4 getViewMatrix() const;
    void processMouseMovement(float xoffset, float yoffset);
	void onSnapshot(NetPlayerMove &pkt, const Renderer &world, int32_t clientTick);
	Snapshot getLatestSnapshot() { return getPlayer()->snapshots.empty() ? Snapshot{} : getPlayer()->snapshots.back(); }

	void drawWireframeSelectedBlockFace(std::shared_ptr<Renderer> &Renderer, glm::mat4 &view, glm::mat4 &projection);

	const inline bool isThirdPersonCameraActive() const {return thirdPersonCamera;}
	const inline void toggleThirdPersonCamera() {thirdPersonCamera = !thirdPersonCamera; player->setDoDraw(thirdPersonCamera);}
	inline std::shared_ptr<ClientPlayer> getPlayer() {return player;};

	void predict(const Renderer &world, int32_t clientTick);
	inline void queueInput(const NetPlayerInputs &inputs, int32_t clientTick) { InputsMap[clientTick] = inputs; }
};

#endif // CAMERA_HPP