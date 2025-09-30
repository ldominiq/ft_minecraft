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

	// //""Temporarily"" put some chunks in Camera.
	// std::unordered_map<ChunkPos, std::shared_ptr<ChunkRenderer>> chunks;

	PlayerMovement movement;
	// int32_t currTick = 0;
	// int32_t serverCurrTick = 0;

	int64_t amountOfSnapshotsReceived = 0;
	glm::vec3 predictedPosition;
	glm::vec3 previousPosition;

	double serverTick = 0;
	double prevServerTick = 0;

public:
	std::vector<NetPlayerInputs> inputsList;

    float MouseSensitivity;

	uint8_t loadRadius = 12; // 4 - 32

    explicit Camera(glm::vec3 position);
	~Camera();

    glm::mat4 getViewMatrix() const;
    void processMouseMovement(float xoffset, float yoffset);
	void onSnapshot(NetPlayerMove &pkt, const Renderer &world);
	void lerpToNextPosition(float time);
	// void updatePosition(NetPlayerMove &pkt);

	inline const float getYaw() const { return movement.yaw; }
	inline const float getPitch() const { return movement.pitch; }
	inline const uint8_t getLoadRadius() const { return loadRadius; }
	inline int tickDiff(int clientTick, int serverTick) { return clientTick - serverTick; }
	// inline void tick() {currTick++; }
	// inline int32_t getTick() const { return currTick; }

	inline const glm::vec3 getPosition() const { return movement.getPosition(); }
	inline const glm::vec3 getCameraDir() const { return movement.getCameraDir(); }
	inline const int64_t getAmountOfSnapsReceived() const { return amountOfSnapshotsReceived;}
	// inline void updateCameraVectors(float yaw, float pitch) {movement.updateCameraVectors(yaw, pitch); }
	// inline void calculateNewPosition(const Renderer &world) {movement.calculateNewPosition(world); }

	inline void setPosition(glm::vec3 position) { movement.setPosition(position); }

	void drawWireframeSelectedBlockFace(std::unique_ptr<Renderer> &Renderer, glm::mat4 &view, glm::mat4 &projection);
};


#endif // CAMERA_HPP