#ifndef CAMERA_HPP
#define CAMERA_HPP

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <memory>

#include "Chunk.hpp"
#include "Shader.hpp"
#include "Protocol.hpp"
#include "Rendering.hpp"

class Camera {

	GLuint wireframeVAO, wireframeVBO, wireframeEBO;

	void initWireframeCube();
	std::unique_ptr<Shader> blockWireframeShader = nullptr;

	//""Temporarily"" put some chunks in Camera.
	std::unordered_map<ChunkPos, std::shared_ptr<Chunk>> chunks;

public:
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    float Yaw, Pitch;
    float MovementSpeed;
    float MouseSensitivity;

	uint8_t loadRadius = 12; // 4 - 32

    explicit Camera(glm::vec3 position);

    glm::mat4 getViewMatrix() const;
    void processMouseMovement(float xoffset, float yoffset);
    void updateCameraVectors();
	void updatePosition(NetPlayerMove &pkt);

	inline const float getYaw() const { return Yaw; }
	inline const float getPitch() const { return Pitch; }
	inline const uint8_t getLoadRadius() const { return loadRadius; }

	
	bool getTargetedBlock(std::unique_ptr<Rendering> &rendering, glm::ivec3& hitBlock, glm::ivec3& faceNormal, float maxDistance = 100); //faceNormal is currently unused
	void drawWireframeSelectedBlockFace(std::unique_ptr<Rendering> &rendering, glm::mat4 &view, glm::mat4 &projection);
};


#endif // CAMERA_HPP