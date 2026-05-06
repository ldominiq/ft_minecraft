
#include "PlayerMovement.hpp"
#include "CommonWorld.hpp"

PlayerMovement::PlayerMovement():	LivingEntity(glm::vec3(0.0f, 0.0f, 0.0f))
{
	name = "nameless";

	type = PLAYER;
	this->velocity = glm::vec3(0.0f, 0.0f, 0.0f);
	this->entityWidth = 0.6f;
	this->entityHeight = 1.8f;

    this->Front = glm::vec3(0.0f, 0.0f, -1.0f); //not really needed
	spawnPosition = position;
	yaw = 0;
	pitch = 0;

	eyesheight = entityHeight - forehead;
}

//client
PlayerMovement::PlayerMovement(const glm::vec3 &position, float yaw, entityID ID): LivingEntity(position, yaw ,ID)
{
	name = "nameless";

	type = PLAYER;

	this->entityWidth = 0.6f;
	this->entityHeight = 1.8f;

	eyesheight = entityHeight - forehead;
}

PlayerMovement::~PlayerMovement()
{
}

void PlayerMovement::onDeath()
{
	setPosition(spawnPosition); // TODO : maybe add a respawn delay and play death animation instead of instant teleport?
	positionUpdated = true;
	health = 20;
	velocity = glm::vec3(0.0f);
	accumulatedFallDistance = 0.0f;
	jumpBoostApplied = false;
	pendingInputs.clear();
	pendingArmSwing = false;
}

//for creative
void PlayerMovement::updatePosition()
{
    NetPlayerInputs inputs = lastInputsPktRecvd;

    this->movementSpeed = (inputs.keys & IN_RUN) ? FLY_SPEED : DEFAULT_SPEED;

    float deltaTime = MS_TICK_RATE / 1000.0f;
    float velocity = this->movementSpeed * deltaTime;

    glm::vec3 prevPosition = this->position;

    // --- Local movement input ---
    float lx = 0.0f;
    float lz = 0.0f;

    if (inputs.keys & IN_FORWARD)  lx += 1.0f;
    if (inputs.keys & IN_BACKWARD) lx -= 1.0f;
    if (inputs.keys & IN_LEFT)     lz -= 1.0f;
    if (inputs.keys & IN_RIGHT)    lz += 1.0f;

    glm::vec2 inputDir(lx, lz);
    if (glm::length(inputDir) > 0.0f)
        inputDir = glm::normalize(inputDir);

    // --- Rotate by yaw ---
    float yawRad = glm::radians(yaw);

    glm::vec2 worldDir;
    worldDir.x = inputDir.x * std::cos(yawRad) - inputDir.y * std::sin(yawRad);
    worldDir.y = inputDir.x * std::sin(yawRad) + inputDir.y * std::cos(yawRad);

    // --- Apply movement ---
    this->position.x += worldDir.x * velocity;
    this->position.z += worldDir.y * velocity;

    // Vertical movement
    if (inputs.keys & IN_UP)
        this->position.y += velocity;

    if (inputs.keys & IN_DOWN)
        this->position.y -= velocity;

    if (prevPosition != this->position)
        positionUpdated = true;
}

void PlayerMovement::updateCameraVectors() {
	glm::vec3 front{};
	front.x = cos(glm::radians(yaw)) * cos(glm::radians(pitch));
	front.y = sin(glm::radians(pitch));
	front.z = sin(glm::radians(yaw)) * cos(glm::radians(pitch));
	this->Front = glm::normalize(front);
	this->Right = glm::normalize(glm::cross(this->Front, this->WorldUp));
}

void PlayerMovement::applyFallDamage()
{
	if (gamemode == GAMEMODES::SURVIVAL)
		LivingEntity::applyFallDamage();
}

void PlayerMovement::doJump(const ICommonWorld &world)
{
	// Determine if on ground by testing a tiny epsilon below feet
	AABB boxFeetProbe = this->constructAABB(glm::vec3(this->position.x, this->position.y -EPS - 0.01f, this->position.z));
	onGround = this->aabbCollidesWithWorld(boxFeetProbe, world);

	// jump
	this->jump = lastInputsPktRecvd.keys & IN_UP;
	const float JUMP_EPS = 0.01f; // TODO (when physics (with pred) work) RECHECK THIS IS USEFUL
	if (this->jump && onGround && this->velocity.y <= JUMP_EPS) {
		this->velocity.y = JUMP_VELOCITY;
		jumpBoostApplied = false;
	}
}

glm::vec3 PlayerMovement::getDesiredMove()
{
	NetPlayerInputs inputs = lastInputsPktRecvd;

	float effectMultiplier = 1.0f;
	float slipperiness = onGround ? SM_DEFAULT : SM_AIRBORNE; 

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
	float accelAir    = 0.02f * movementMultiplier;
	float accel = onGround ? accelGround : accelAir;

	glm::vec2 accelVec = inputWorld * accel * (hasMovementInput ? 1.0f : 0.0f);

	glm::vec2 springBoost(0.0f);
	if (this->jump && (inputs.keys & IN_RUN) && !jumpBoostApplied) {
		springBoost = glm::vec2(std::cos(yawRad), std::sin(yawRad)) * (movementMultiplier == MM_SPRINTING ? 0.2f : 0.0f) * (hasMovementInput && !(lx < 0) ? 1.0f : 0.0f);
		jumpBoostApplied = true;
	}

	glm::vec2 newV = momentum + accelVec + springBoost;

	slipperiness_prev = slipperiness;

	this->velocity.x = newV.x;
	this->velocity.z = newV.y;

	if (std::abs(this->velocity.x) < EPS) this->velocity.x = 0;
	if (std::abs(this->velocity.z) < EPS) this->velocity.z = 0;

	return glm::vec3(this->velocity.x, 0.0f, this->velocity.z);
}

void PlayerMovement::calculateUnderwaterPosition(const ICommonWorld &world)
{
	accumulatedFallDistance = 0.0f;

    NetPlayerInputs inputs = lastInputsPktRecvd;

    float speed = 0.06f;
    float drag = 0.85f;

    // Build forward vector from camera
    glm::vec3 forward;
    forward.x = std::cos(glm::radians(pitch)) * std::cos(glm::radians(yaw));
    forward.y = inputs.keys & IN_UP ? 0.0f : std::sin(glm::radians(pitch));
    forward.z = std::cos(glm::radians(pitch)) * std::sin(glm::radians(yaw));
    forward = glm::normalize(forward);

    // Right vector (for strafing)
    glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

    glm::vec3 accel(0.0f);

    if (inputs.keys & IN_FORWARD)  accel += forward;
    if (inputs.keys & IN_BACKWARD) accel -= forward;
    if (inputs.keys & IN_LEFT)     accel -= right;
    if (inputs.keys & IN_RIGHT)    accel += right;

    if (glm::length(accel) > 0.0f)
        accel = glm::normalize(accel) * speed;

    // Apply drag (water resistance)
    this->velocity *= drag;

    // Apply movement input
    this->velocity += accel;

    // Idle sinking
    if (glm::length(accel) == 0.0f)
    {
        this->velocity.y -= 0.02f; // slow fall
    }

	if (inputs.keys & IN_UP)
		this->velocity.y += 0.045f;

    // Clamp overall speed
    float maxSpeed = 0.4f;
    if (glm::length(this->velocity) > maxSpeed)
        this->velocity = glm::normalize(this->velocity) * maxSpeed;

    // Check collisions and apply final position
	glm::vec3 pos = this->position;

	// --- X axis ---
	glm::vec3 tryX = pos + glm::vec3(this->velocity.x, 0.0f, 0.0f);
	if (!aabbCollidesWithWorld(constructAABB(tryX), world)) {
		pos.x = tryX.x;
	} else {
		this->velocity.x = 0.0f;
	}

	// --- Y axis ---
	glm::vec3 tryY = pos + glm::vec3(0.0f, this->velocity.y, 0.0f);
	if (!aabbCollidesWithWorld(constructAABB(tryY), world)) {
		pos.y = tryY.y;
	} else {
		this->velocity.y = 0.0f;
	}

	// --- Z axis ---
	glm::vec3 tryZ = pos + glm::vec3(0.0f, 0.0f, this->velocity.z);
	if (!aabbCollidesWithWorld(constructAABB(tryZ), world)) {
		pos.z = tryZ.z;
	} else {
		this->velocity.z = 0.0f;
	}

	setPosition(pos);
}

void PlayerMovement::calculateNewPosition(const ICommonWorld &world)
{

	auto updatePos = [this, &world]()
	{
		if (gamemode == GAMEMODES::SURVIVAL) {
			if (world.isUnderwater(this->position))
				calculateUnderwaterPosition(world);
			else
			{
				doJump(world);
				glm::vec3 desiredMove = getDesiredMove();
				this->calculateNewXZPosition(world, desiredMove);
				this->calculateNewYPosition(world);
			}
		} else if (gamemode == GAMEMODES::SPECTATOR) {
			updatePosition();
		}
		this->jump = false;
	};

	if (skipDuplicateInputs) {
		// Server path: drain the per-player input queue, running one physics step per
		// queued input.  When the client sends N inputs in a single frame (low FPS
		// catch-up), all N packets end up here and each gets its own step — keeping
		// server and client tick counts in sync instead of the server skipping to the
		// last input and being N-1 ticks behind.
		if (pendingInputs.empty())
			return;

		constexpr int kMaxCatchup = 6;
		int processed = 0;
		while (!pendingInputs.empty() && processed < kMaxCatchup) {
			lastInputsPktRecvd = pendingInputs.front();
			pendingInputs.pop_front();
			lastAppliedServerClientReconciliationTick = lastInputsPktRecvd.serverClientReconciliationTick;
			setYawAndPitch(lastInputsPktRecvd.yaw, lastInputsPktRecvd.pitch);
			updateCameraVectors();

			updatePos();

			processed++;
		}
		this->hasHorizontalInput =
			(lastInputsPktRecvd.keys & (IN_FORWARD | IN_BACKWARD | IN_LEFT | IN_RIGHT)) != 0;
	} else {
		// Client path: original single-step behavior used by prediction and replay.
		lastAppliedServerClientReconciliationTick = lastInputsPktRecvd.serverClientReconciliationTick;

		this->hasHorizontalInput =
			(lastInputsPktRecvd.keys & (IN_FORWARD | IN_BACKWARD | IN_LEFT | IN_RIGHT)) != 0;

		updatePos();
	}
}
