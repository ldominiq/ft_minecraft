#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>
#include <memory>
#include <ranges>

#include "ChunkRenderer.hpp"
#include "Shader.hpp"
#include "Protocol.hpp"
#include "Renderer.hpp"
#include "PlayerMovement.hpp"
#include "GLFW/glfw3.h"


class Camera {

	GLuint wireframeVAO, wireframeVBO, wireframeEBO;

	void initWireframeCube();
	void predictNTicks(const Renderer &world);
	std::unique_ptr<Shader> blockWireframeShader = nullptr;

	int64_t amountOfSnapshotsReceived = 0;
	glm::vec3 predictedPosition;
	glm::vec3 previousPosition;

	//TODO : get all the tick logic elsewhere;
	float serverTick = 0;
	float prevServerTick = 0;

public:
	PlayerMovement movement;
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

	inline const uint8_t getLoadRadius() const { return loadRadius; }
	// inline int tickDiff(int clientTick, int serverTick) { return clientTick - serverTick; }

	inline const int64_t getAmountOfSnapsReceived() const { return amountOfSnapshotsReceived;}

	void drawWireframeSelectedBlockFace(std::unique_ptr<Renderer> &Renderer, glm::mat4 &view, glm::mat4 &projection);
};


#endif // CAMERA_HPP