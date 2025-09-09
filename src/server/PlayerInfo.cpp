
#include "PlayerInfo.hpp"
#include "World.hpp"

// TODO : FIX BUG WHERE YOU CAN'T JUMP WHEN PRESSING MULTIPLE KEYS (and probably other actions blocked when pressing too many inputs)

CPlayerInfo::CPlayerInfo(): LivingEntity(glm::vec3(0, 150, 0)),
      yaw(0.0f), pitch(0.0f), loadRadius(12)
{
	entityWidth = 0.6f;
	entityHeight = 1.8f;
    Front = glm::vec3(0.0f, 0.0f, -1.0f);
}

void CPlayerInfo::updatePosition()
{
	NetPlayerInputs inputs = lastInputsPktRecvd;

	yaw = inputs.yaw;
	pitch = inputs.pitch;
	loadRadius = inputs.loadRadius;

    movementSpeed = (inputs.keys & IN_RUN) ? FLY_SPEED : DEFAULT_SPEED;

	float deltaTime = 0.05f;
	float velocity = movementSpeed * deltaTime;

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
		position.y += WorldUp.y * velocity;
	if (inputs.keys & IN_DOWN)
		position.y -= WorldUp.y * velocity; 
}

void CPlayerInfo::updateCameraVectors(float yaw, float pitch) {
    glm::vec3 front;
	this->yaw = yaw;
	this->pitch = pitch;
    front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
    front.y = sin(glm::radians(pitch));
    front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
    Front = glm::normalize(front);
    Right = glm::normalize(glm::cross(Front, WorldUp));
    Up    = glm::normalize(glm::cross(Right, Front));
}

void CPlayerInfo::doJump(const std::unique_ptr<World> &world)
{
	// Determine if on ground by testing a tiny epsilon below feet
	AABB boxFeetProbe = constructAABB(glm::vec3(position.x, position.y -EPS - 0.01f, position.z));
	bool onGround = aabbCollidesWithWorld(boxFeetProbe, world);

	// jump
	jump = lastInputsPktRecvd.keys & IN_UP;
	const float JUMP_EPS = 0.01f; // TODO (when physics work) RECHECK THIS IS USEFUL
	if (jump && onGround && verticalVelocity <= JUMP_EPS) {
		verticalVelocity = std::sqrt(2.0f * GRAVITY * 1/3.0f * 1.1025f * DRAG); //JUMP_VELOCITY;
		onGround = false;
		jumpBoostApplied = false;
	}
}

// glm::vec3 CPlayerInfo::getDesiredMove()
// {
//     NetPlayerInputs inputs = lastInputsPktRecvd;

//     bool ground = true;  
 
//     float effectMultiplier = 1.0f; 
//     float slipperiness = SM_DEFAULT; 
//     float slipperiness_prev = slipperiness;

//     float movementMultiplier = MM_WALKING;
//     if (inputs.keys & IN_RUN) movementMultiplier = MM_SPRINTING;

// 	//x and z inverted for some obscure reason
//     float lx = 0.0f, lz = 0.0f;
//     if (inputs.keys & IN_FORWARD)  lx += 1.0f;
//     if (inputs.keys & IN_BACKWARD) lx -= 1.0f;
//     if (inputs.keys & IN_LEFT)     lz -= 1.0f;
//     if (inputs.keys & IN_RIGHT)    lz += 1.0f;

//     bool hasMovementInput = (std::abs(lx) > 0.0f || std::abs(lz) > 0.0f);

//     glm::vec2 inputDir(lx, lz);
//     if (glm::length(inputDir) > 0.0f) inputDir = glm::normalize(inputDir);

//     float yawRad = glm::radians(yaw);
//     glm::vec2 inputWorld;
//     inputWorld.x = inputDir.x * std::cos(yawRad) - inputDir.y * std::sin(yawRad);
//     inputWorld.y = inputDir.x * std::sin(yawRad) + inputDir.y * std::cos(yawRad);

//     glm::vec2 prevV(velocity.x, velocity.z);
//     glm::vec2 momentum = prevV * (slipperiness_prev * 0.91f);

//     float accelGround = 0.1f * movementMultiplier * effectMultiplier * std::pow(0.6f / slipperiness, 3.0f);
//     float accelAir    = 0.02f * movementMultiplier * effectMultiplier;
//     float accel = ground ? accelGround : accelAir;

//     glm::vec2 accelVec = inputWorld * accel * (hasMovementInput ? 1.0f : 0.0f);

// 	glm::vec2 sprintBoost(0.0f);
// 	if (jump && (inputs.keys & IN_RUN) && !jumpBoostApplied) {
// 		sprintBoost = glm::vec2(std::cos(yawRad), std::sin(yawRad)) * 0.2f;
// 		jumpBoostApplied = true;
// 	}

//     glm::vec2 newV = momentum + accelVec + sprintBoost;
//     velocity.x = newV.x;
//     velocity.z = newV.y;

//     return glm::vec3(velocity.x, 0.0f, velocity.z);
// }

glm::vec3 CPlayerInfo::getDesiredMove()
{
	int tps = 60;
    NetPlayerInputs inputs = lastInputsPktRecvd;

    bool ground = true;  
 
    float effectMultiplier = 1.0f; 
    float slipperiness = SM_DEFAULT; 
    float slipperiness_prev = slipperiness;

    float movementMultiplier = MM_WALKING;
    if (inputs.keys & IN_RUN) movementMultiplier = MM_SPRINTING;

    // scaling for TPS differences (vanilla assumes 20 TPS)
    float baseTPS = 20.0f;
    float tickScale = baseTPS / float(tps);   // e.g. 20/60 = 0.333...
    float tickPow   = tickScale;              // used as exponent for multipliers

    // x and z inverted for some obscure reason
    float lx = 0.0f, lz = 0.0f;
    if (inputs.keys & IN_FORWARD)  lx += 1.0f;
    if (inputs.keys & IN_BACKWARD) lx -= 1.0f;
    if (inputs.keys & IN_LEFT)     lz -= 1.0f;
    if (inputs.keys & IN_RIGHT)    lz += 1.0f;

    bool hasMovementInput = (std::abs(lx) > 0.0f || std::abs(lz) > 0.0f);

    glm::vec2 inputDir(lx, lz);
    if (glm::length(inputDir) > 0.0f) 
        inputDir = glm::normalize(inputDir);

    float yawRad = glm::radians(yaw);
    glm::vec2 inputWorld;
    inputWorld.x = inputDir.x * std::cos(yawRad) - inputDir.y * std::sin(yawRad);
    inputWorld.y = inputDir.x * std::sin(yawRad) + inputDir.y * std::cos(yawRad);

    glm::vec2 prevV(velocity.x, velocity.z);

    // friction: scale with power (0.91^tickScale)
    glm::vec2 momentum = prevV * std::pow(slipperiness_prev * 0.91f, tickScale);

    // accelerations: divide constants by TPS ratio
    float accelGround = (0.1f * tickScale) * movementMultiplier * effectMultiplier 
                        * std::pow(0.6f / slipperiness, 3.0f);

    float accelAir    = (0.02f * tickScale) * movementMultiplier;

    float accel = ground ? accelGround : accelAir;

    glm::vec2 accelVec = inputWorld * accel * (hasMovementInput ? 1.0f : 0.0f);

    glm::vec2 sprintBoost(0.0f);
    if (jump && (inputs.keys & IN_RUN) && !jumpBoostApplied) {
        // sprint boost: scale by tickScale
        sprintBoost = glm::vec2(std::cos(yawRad), std::sin(yawRad)) * std::pow(0.2f, 3.0f);
        jumpBoostApplied = true;
    }

    glm::vec2 newV = momentum + sprintBoost;
    velocity.x = newV.x;
    velocity.z = newV.y;

    return glm::vec3(velocity.x, 0.0f, velocity.z);
}

void CPlayerInfo::calculateNewPosition(const std::unique_ptr<World> &world)
{
	float headHeight = entityHeight - 0.3f; // TODO: rethink this
	position.y -= headHeight;

	if (gamemode == GAMEMODES::SURVIVAL)
	{
		doJump(world);
		glm::vec3 desiredMove = getDesiredMove();
		calculateNewXZPosition(world, desiredMove);
		calculateNewYPosition(world);
	}
	else if (gamemode == GAMEMODES::SPECTATOR)
	{
		updatePosition();
	}
	position.y += headHeight;

	lastInputsPktRecvd = NetPlayerInputs();
	jump = false;
}