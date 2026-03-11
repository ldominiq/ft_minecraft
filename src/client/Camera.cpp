#include "Camera.hpp"

Camera::Camera(glm::vec3 position)
    : MouseSensitivity(0.1f) {

	//TODO position & yaw should be given by server
	glm::vec3 startingPos = position; // UNUSED
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
	if (!thirdPersonCamera)
		return glm::lookAt(player->getPosition(), player->getPosition() + player->Front, player->WorldUp);

	float cameraDistance = 3.0f;  // behind the player
	float cameraHeight   = 1.5f;  // slightly above

    glm::vec3 playerPos = player->getPosition();

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

void Camera::lerpToNextPosition(float deltaTime)
{
	if (prevServerTick == 0) return ;

	float currTime = prevServerTick + deltaTime * 1000;
	currTime = std::clamp(currTime, prevServerTick, serverTick);
	float intraTick = (currTime - prevServerTick) / (serverTick - prevServerTick);

	// std::cout << deltaTime << std::endl;
	// std::cout << currTime << std::endl;
	// std::cout << prevServerTick << std::endl;
	// std::cout << serverTick<< std::endl;
	// std::cout << intraTick << std::endl;
	// std::cout << std::endl;
	glm::vec3 renderPos = player->prevPosition + (player->nextPosition - player->prevPosition) * intraTick;
	player->setPosition(renderPos);
}
		
glm::vec3 Camera::lerpEntityToNextPosition(float deltaTime, const glm::vec3 &prevPosition, const glm::vec3 &nextPosition)
{
	if (prevPosition == glm::vec3{}) return nextPosition;

	float currTime = prevServerTick + deltaTime * 1000;
	currTime = std::clamp(currTime, prevServerTick, serverTick);
	float intraTick = (currTime - prevServerTick) / (serverTick - prevServerTick);

	glm::vec3 renderPos = prevPosition + (nextPosition - prevPosition) * intraTick;
	return renderPos;
}

// Remove prediction for now. 
void Camera::predictNTicks(const Renderer &world)
{
	(void)world;

	// previousPosition = predictedPosition;
	// static int diff;
	// diff = serverCurrTick ? tickDiff(currTick, serverCurrTick) : diff; //assumes ping remains constant... this whole logic is... frail
	// serverCurrTick = currTick - diff;

	// for(auto itr = inputsList.cbegin(); itr != inputsList.cend();) {
	// if (itr->tick < serverCurrTick) {
	// 	itr = inputsList.erase(itr);
	// } else
	// 	++itr;
	// }

	// for(int i = 0; i < diff - 1; i++)
	// {
	// 	if (inputsList.size() > i)
	// 		movement.lastInputsPktRecvd = inputsList[i];
	// }
	// predictedPosition = movement.getPosition();
	// movement.setPosition(previousPosition);
}

void Camera::onSnapshot(NetPlayerMove &pkt, const Renderer &world)
{
	amountOfSnapshotsReceived++;

	glm::vec3 position;
	position.x = pkt.positionX;
	position.y = pkt.positionY;
	position.z = pkt.positionZ;

	glm::vec3 velocity;
	velocity.x = pkt.velocityX;
	velocity.y = pkt.velocityY;
	velocity.z = pkt.velocityZ;
	player->setVelocity(velocity);

	// movement.setPosition(position);
	player->prevPosition = player->nextPosition;
	player->nextPosition = position;

	prevServerTick = serverTick;
	serverTick = pkt.serverTick * MS_TICK_RATE;

	predictNTicks(world);
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

	if (!Renderer->getTargetedBlock(player->getPosition(), glm::normalize(player->Front), blockPos, faceNormal))
		return ;

	glm::mat4 model = glm::translate(glm::mat4(1.0f), glm::vec3(blockPos));

	blockWireframeShader->use();
	blockWireframeShader->setMat4("model", model);
	blockWireframeShader->setMat4("view", view);
	blockWireframeShader->setMat4("projection", projection);

    glBindVertexArray(wireframeVAO);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_INT, nullptr);
	glBindVertexArray(0);
}
