
#include "PlayerInfo.hpp"

CPlayerInfo::CPlayerInfo(): position(glm::vec3(0,0,0)), WorldUp(0.0f, 1.0f, 0.0f),
      yaw(45.0f), pitch(0.0f), movementSpeed(5.0f), loadRadius(12)
{
    Front = glm::vec3(0.0f, 0.0f, -1.0f);
}

void CPlayerInfo::updatePosition(NetPlayerInputs &inputs, float &deltaTime)
{
	yaw = inputs.yaw;
	pitch = inputs.pitch;
	loadRadius = inputs.loadRadius;
	updateCameraVectors();

	float velocity = movementSpeed * deltaTime * ((inputs.keys & IN_RUN) ? ACCEL : 1.0f);

    // Minecraft'ish camera. Doens't move along the Y axis
    glm::vec3 horizontalFront = glm::normalize(glm::vec3(Front.x, 0.0f, Front.z));

	// 4 directions
    if (inputs.keys & IN_FORWARD)
        position += horizontalFront * velocity;
    if (inputs.keys & IN_BACKWARD)
        position -= horizontalFront * velocity;
    if (inputs.keys & IN_LEFT)
        position -= glm::normalize(glm::cross(horizontalFront, WorldUp)) * velocity;
    if (inputs.keys & IN_RIGHT)
        position += glm::normalize(glm::cross(horizontalFront, WorldUp)) * velocity;

	// Up and Down
	if (inputs.keys & IN_UP)
		position.y += 1.0f;
	if (inputs.keys & IN_DOWN)
		position.y -= 1.0f;
}

void CPlayerInfo::updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    Front = glm::normalize(front);
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up    = glm::normalize(glm::cross(Right, Front));
}