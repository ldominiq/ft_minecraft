#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>
#include <memory>
#include <ranges>

#include "Protocol.hpp"
#include "Renderer.hpp"
#include "GLFW/glfw3.h"
#include "ClientPlayer.hpp"

class Camera {

	GLuint wireframeVAO, wireframeVBO, wireframeEBO;

	void initWireframeCube();
	void predictNTicks(const Renderer &world);
	std::unique_ptr<Shader> blockWireframeShader = nullptr;

	int64_t amountOfSnapshotsReceived = 0;

	//TODO : get all the tick logic elsewhere;
	float serverTick = 0;
	float prevServerTick = 0;

	std::shared_ptr<ClientPlayer> player;
	bool thirdPersonCamera = false;

public:
	std::vector<NetPlayerInputs> inputsList;

    float MouseSensitivity;

	uint8_t loadRadius = 12; // 4 - 32

    explicit Camera(glm::vec3 position);
	~Camera();

    glm::mat4 getViewMatrix() const;
    void processMouseMovement(float xoffset, float yoffset);
	void onSnapshot(NetPlayerMove &pkt, const Renderer &world);
	void lerpToNextPosition(float deltaTime);

	//maybe refactor some day and put somewhere else
	glm::vec3 lerpEntityToNextPosition(float deltaTime, const glm::vec3 &prevPosition, const glm::vec3 &nextPosition);

	inline uint8_t getLoadRadius() const { return loadRadius; }
	// inline int tickDiff(int clientTick, int serverTick) { return clientTick - serverTick; }

	inline int64_t getAmountOfSnapsReceived() const { return amountOfSnapshotsReceived;}

	void drawWireframeSelectedBlockFace(std::shared_ptr<Renderer> &Renderer, glm::mat4 &view, glm::mat4 &projection);

	inline bool isThirdPersonCameraActive() const {return thirdPersonCamera;}
	inline void toggleThirdPersonCamera() {thirdPersonCamera = !thirdPersonCamera; player->setDoDraw(thirdPersonCamera);}
	const inline std::shared_ptr<ClientPlayer> getPlayer() {return player;};
};


#endif // CAMERA_HPP