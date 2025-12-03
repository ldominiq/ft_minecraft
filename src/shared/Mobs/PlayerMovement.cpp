
#include "PlayerMovement.hpp"

PlayerMovement::PlayerMovement(const glm::vec3 &position):	LivingEntity(position)
{
	type = PLAYER;
	this->velocity = glm::vec3(0.0f, 0.0f, 0.0f);
	this->entityWidth = 0.6f;
	this->entityHeight = 1.8f;
    this->Front = glm::vec3(0.0f, 0.0f, -1.0f); //not really needed
	yaw = 0;
	pitch = 0;
}

//client
PlayerMovement::PlayerMovement(const glm::vec3 &position, float yaw, entityID ID): LivingEntity(position, yaw ,ID)
{
	type = PLAYER;
}

PlayerMovement::~PlayerMovement()
{
}

//for creative
void PlayerMovement::updatePosition()
{
	NetPlayerInputs inputs = lastInputsPktRecvd;

    this->movementSpeed = (inputs.keys & IN_RUN) ? FLY_SPEED : DEFAULT_SPEED;

	float deltaTime = MS_TICK_RATE/1000.0f;
	float velocity = this->movementSpeed * deltaTime;

    // Minecraft'ish camera. Doesn't move along the Y axis
    glm::vec3 horizontalFront = glm::normalize(glm::vec3(this->Front.x, 0.0f, this->Front.z));

	glm::vec3 prevPosition = this->position;

	// 4 directions
    if (inputs.keys & IN_FORWARD)
        this->position += horizontalFront * velocity;
    if (inputs.keys & IN_BACKWARD)
        this->position -= horizontalFront * velocity;
    if (inputs.keys & IN_LEFT)
        this->position -= glm::normalize(glm::cross(horizontalFront, this->WorldUp)) * velocity;
    if (inputs.keys & IN_RIGHT)
        this->position += glm::normalize(glm::cross(horizontalFront, this->WorldUp)) * velocity;

	// Up and Down
	if (inputs.keys & IN_UP)
		this->position.y += this->WorldUp.y * velocity;
	if (inputs.keys & IN_DOWN)
		this->position.y -= this->WorldUp.y * velocity; 

	if (prevPosition != this->position)
		positionUpdated = true;
}

void PlayerMovement::updateCameraVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    this->Front = glm::normalize(front);
    this->Right = glm::normalize(glm::cross(this->Front, this->WorldUp));
}

void PlayerMovement::doJump(const ICommonWorld &world)
{
	// Determine if on ground by testing a tiny epsilon below feet
	AABB boxFeetProbe = this->constructAABB(glm::vec3(this->position.x, this->position.y -EPS - 0.01f, this->position.z));
	bool onGround = this->aabbCollidesWithWorld(boxFeetProbe, world);

	// jump
	this->jump = lastInputsPktRecvd.keys & IN_UP;
	const float JUMP_EPS = 0.01f; // TODO (when physics (with pred) work) RECHECK THIS IS USEFUL
	if (this->jump && onGround && this->velocity.y <= JUMP_EPS) {
		this->velocity.y = JUMP_VELOCITY;
		this->onGround = false;
		jumpBoostApplied = false;
	}
}

glm::vec3 PlayerMovement::getDesiredMove()
{
    NetPlayerInputs inputs = lastInputsPktRecvd;

    bool ground = true;  
 
    float effectMultiplier = 1.0f; 
    float slipperiness = SM_DEFAULT; 
    float slipperiness_prev = slipperiness;

    float movementMultiplier = MM_WALKING;
    if (inputs.keys & IN_RUN) movementMultiplier = MM_SPRINTING;

	//x and z inverted for some obscure reason
    float lx = 0.0f, lz = 0.0f;
    if (inputs.keys & IN_FORWARD)  lx += 1.0f;
    if (inputs.keys & IN_BACKWARD) lx -= 1.0f;
    if (inputs.keys & IN_LEFT)     lz -= 1.0f;
    if (inputs.keys & IN_RIGHT)    lz += 1.0f;

    bool hasMovementInput = (std::abs(lx) > 0.0f || std::abs(lz) > 0.0f);

    glm::vec2 inputDir(lx, lz);
    if (glm::length(inputDir) > 0.0f) inputDir = glm::normalize(inputDir);

    float yawRad = glm::radians(yaw);
    glm::vec2 inputWorld;
    inputWorld.x = inputDir.x * std::cos(yawRad) - inputDir.y * std::sin(yawRad);
    inputWorld.y = inputDir.x * std::sin(yawRad) + inputDir.y * std::cos(yawRad);

    glm::vec2 prevV(this->velocity.x, this->velocity.z);
    glm::vec2 momentum = prevV * (slipperiness_prev * 0.91f);

    float accelGround = 0.1f * movementMultiplier * effectMultiplier * std::pow(0.6f / slipperiness, 3.0f);
    float accelAir    = 0.02f * movementMultiplier * effectMultiplier;
    float accel = ground ? accelGround : accelAir;

    glm::vec2 accelVec = inputWorld * accel * (hasMovementInput ? 1.0f : 0.0f);

	glm::vec2 sprintBoost(0.0f);
	if (this->jump && (inputs.keys & IN_RUN) && !jumpBoostApplied) {
		sprintBoost = glm::vec2(std::cos(yawRad), std::sin(yawRad)) * 0.2f;
		jumpBoostApplied = true;
	}

    glm::vec2 newV = momentum + accelVec + sprintBoost;

    this->velocity.x = newV.x;
    this->velocity.z = newV.y;

    return glm::vec3(this->velocity.x, 0.0f, this->velocity.z);
}

void PlayerMovement::calculateNewPosition(const ICommonWorld &world)
{
	constexpr float forehead = 0.3f;
	float headHeight = this->entityHeight - forehead;
	this->position.y -= headHeight;

	// TODO : return early if no new packet to read and velocities are 0 and there is no collision with block under. To avoid doing unnecessary calculations. Do the same with every other entity

	if (gamemode == GAMEMODES::SURVIVAL)
	{
		doJump(world);
		glm::vec3 desiredMove = getDesiredMove();
		this->calculateNewXZPosition(world, desiredMove);
		this->calculateNewYPosition(world);
	}
	else if (gamemode == GAMEMODES::SPECTATOR)
	{
		updatePosition();
	}
	this->position.y += headHeight;

	lastInputsPktRecvd = {};
	this->jump = false;
}