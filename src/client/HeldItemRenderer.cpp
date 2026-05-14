
#include "HeldItemRenderer.hpp"

#include <cmath>
#include <cstring>

#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include "blockRenderingHelperFunctions.hpp"
#include "TextureManager.hpp"
#include "IClientEntity.hpp"
#include "Character.hpp"
#include "Item.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

// Per-vertex stride in floats (pos.xyz, uv.xy, texLayer) — must match the
// cubePropShader vertex layout and buildCube/buildItemSprite output.
constexpr int STRIDE = 6;

// "Presenting hand" anchor in player-local coords (X=forward, Y=up, Z=right).
// Not at the visible hand (which hangs at hip level on Steve's straight arm) —
// instead the natural held-item position you'd hold a block at to look at it.
// Shoulder is at SHOULDER_HEIGHT; an L_EFF-long "imaginary arm" hangs below it,
// then the item gets a permanent forward bias so it sits in front of the body.
constexpr float SHOULDER_HEIGHT = 1.30f;
constexpr float SHOULDER_RIGHT  = 0.42f;
constexpr float ARM_REST_LEN    = 0.25f; // short → keeps the item near chest
constexpr float HAND_FWD_BIAS   = 0.28f; // forward of body, doesn't rotate
constexpr float CUBE_SCALE      = 0.42f;
constexpr float SPRITE_SCALE    = 0.42f;

// First-person viewmodel placement in camera space (X right, Y up, -Z forward).
// Pushed far right and down so it lives in the bottom-right corner like a real
// Minecraft viewmodel, with VM_CUBE big enough to read clearly.
constexpr float VM_HAND_X    =  0.95f;
constexpr float VM_HAND_Y    = -0.65f;
constexpr float VM_HAND_Z    = -0.90f;
constexpr float VM_CUBE      =  1.15f; // ~3× original 0.32 — clearly visible
constexpr float VM_SPRITE    =  0.65f;
constexpr float VM_REST_TILT_X = 0.40f; // small downward tilt at rest
constexpr float VM_REST_TILT_Y = 0.40f; // and around Y so we see a 3/4 view
constexpr float VM_SWING_GAIN  = 0.50f; // dampens arm angle so it stays on screen
constexpr float VM_BOB_GAIN_Y  = 0.06f; // 1P walking bob, vertical (camera-space)
constexpr float VM_BOB_GAIN_X  = 0.04f; // 1P walking sway, horizontal

// Combined arm rotation angle for the right shoulder (radians) at the current
// frame. Punches override walks (the character animation does the same).
// Zero when the character isn't animating.
float armAngleFromAnimation(const Character* ch)
{
	if (!ch) return 0.0f;
	if (ch->characterBodyParts.onArmSwingAnimation) {
		// Same shoulder+elbow constants as swingArmAnimation. Summed because
		// the held item is rigidly attached to the forearm, which sees both.
		const float t = ch->characterBodyParts.armSwingPhase;
		const float curve = std::sin(t * static_cast<float>(M_PI));
		return curve * (1.6f + 0.6f);
	}
	if (ch->characterBodyParts.onWalkAnimation) {
		// Right arm in walkAnimation: rotateBodyPart(rightArm, pivot, -angle)
		// → effective rotation = (-angle) * 0.8 (the rotateBodyPart multiplier).
		const float a = std::sin(ch->characterBodyParts.walkPhase * 2.0f * static_cast<float>(M_PI));
		return -a * 0.8f;
	}
	return 0.0f;
}

// Cross-cast helper: LivingEntity and Character are sibling virtual bases of
// IClientEntity, so RTTI handles the LivingEntity* → Character* cross-cast on
// the same most-derived ClientPlayer object.
const Character* asCharacter(const LivingEntity& e)
{
	return dynamic_cast<const Character*>(&e);
}

// Apply a 4×4 model matrix in place to every vertex emitted into `buf` from
// index `startFloat` onward. Touches only the position triplet.
void transformVertsMat(std::vector<float>& buf, std::size_t startFloat,
                       const glm::mat4& M)
{
	for (std::size_t i = startFloat; i + 2 < buf.size(); i += STRIDE) {
		const glm::vec4 p(buf[i + 0], buf[i + 1], buf[i + 2], 1.0f);
		const glm::vec4 q = M * p;
		buf[i + 0] = q.x;
		buf[i + 1] = q.y;
		buf[i + 2] = q.z;
	}
}

// Scale-around-origin + translate (cheaper than the full matrix path).
void transformVerts(std::vector<float>& buf, std::size_t startFloat,
                    float scale, float tx, float ty, float tz)
{
	for (std::size_t i = startFloat; i + 2 < buf.size(); i += STRIDE) {
		buf[i + 0] = buf[i + 0] * scale + tx;
		buf[i + 1] = buf[i + 1] * scale + ty;
		buf[i + 2] = buf[i + 2] * scale + tz;
	}
}

// Compute the held-item anchor (cube center / sprite center) in camera-relative
// world space. Uses a fixed "short arm" hanging from the shoulder; the arm
// rotates by `armAngle` around the player-local Z axis (same axis the visible
// arm rotates around for walks and punches). A permanent forward bias keeps
// the item in front of the body even with the arm vertical.
glm::vec3 itemAnchorRel(const glm::dvec3& worldPos, const glm::dvec3& eyePos,
                        float yawRad, float armAngle)
{
	const float cy = std::cos(yawRad), sy = std::sin(yawRad);

	// Arm vector in player-local coords: rotates with armAngle around +Z.
	// Rest: (0, -L, 0). After R_Z(θ): (sin(θ)*L, -cos(θ)*L, 0).
	const float fwdComp   = std::sin(armAngle) * ARM_REST_LEN + HAND_FWD_BIAS;
	const float upComp    = -std::cos(armAngle) * ARM_REST_LEN;
	const float rightComp = 0.0f;

	// Shoulder in player-local coords: (0, SHOULDER_HEIGHT, SHOULDER_RIGHT).
	// Player-local → world: local x along world forward = (cos,0,sin); local z
	// along world right = (-sin,0,cos). Up = world up.
	const float localFwd   = fwdComp;
	const float localUp    = SHOULDER_HEIGHT + upComp;
	const float localRight = SHOULDER_RIGHT + rightComp;

	const glm::dvec3 worldOffset(
		static_cast<double>(localFwd) * cy + static_cast<double>(localRight) * (-sy),
		static_cast<double>(localUp),
		static_cast<double>(localFwd) * sy + static_cast<double>(localRight) *  cy
	);

	return glm::vec3((worldPos - eyePos) + worldOffset);
}

// Build the model matrix for a held cube: translate to `anchorRel`, rotate
// player-local→world by yaw, apply arm rotation around the player's right
// axis (so the cube tilts with the swing/walk), then center & scale.
glm::mat4 cubeModelMatrix(const glm::vec3& anchorRel, float yawRad,
                          float armAngle, float scale)
{
	glm::mat4 M(1.0f);
	M = glm::translate(M, anchorRel);
	// In this engine yaw=π/2 faces world +Z; GLM's rotate is right-hand around
	// +Y, so we use the negative angle to match.
	M = glm::rotate(M, -yawRad, glm::vec3(0.0f, 1.0f, 0.0f));
	M = glm::rotate(M, armAngle, glm::vec3(0.0f, 0.0f, 1.0f));
	M = glm::scale(M, glm::vec3(scale));
	M = glm::translate(M, glm::vec3(-0.5f));
	return M;
}

// Emit the geometry for one held item into `buf` at the supplied anchor.
// Cubes get the full orient-to-player matrix; sprites/billboards stay
// self-oriented (X-cross is symmetric, billboard reorients per-frame).
bool appendItemMesh(std::vector<float>& buf, const TextureManager* texMgr,
                    uint16_t heldItemType, const glm::vec3& anchorRel,
                    float yawRad, float armAngle,
                    float cubeScale, float spriteScale)
{
	if (heldItemType == 0
	    || heldItemType == static_cast<uint16_t>(BlockType::BEGIN)
	    || heldItemType == static_cast<uint16_t>(BlockType::AIR)) {
		return false;
	}

	ItemType type = itemIDToItemType(heldItemType);
	const std::size_t start = buf.size();

	if (isItemFlat(type)) {
		const int layer = texMgr ? texMgr->getItemSpriteLayer(type) : 0;
		if (auto* b = std::get_if<BlockType>(&type)) {
			(void)b;
			buildItemSprite(buf, 0.0f, 0.0f, 0.0f, layer);
			const float s = spriteScale / 0.25f;
			transformVerts(buf, start, s,
			               anchorRel.x,
			               anchorRel.y - 0.5f * spriteScale,
			               anchorRel.z);
		} else {
			buildItemBillboard(buf, glm::dvec3(anchorRel), layer);
			const float s = spriteScale / 0.25f;
			transformVerts(buf, start, s,
			               anchorRel.x * (1.0f - s),
			               anchorRel.y * (1.0f - s) - 0.5f * spriteScale,
			               anchorRel.z * (1.0f - s));
		}
		return true;
	}

	if (auto* b = std::get_if<BlockType>(&type)) {
		buildCube(buf, 0.0f, 0.0f, 0.0f, 0, 0, *b, texMgr);
		transformVertsMat(buf, start,
		                  cubeModelMatrix(anchorRel, yawRad, armAngle, cubeScale));
		return true;
	}
	return false;
}

glm::dvec3 entityRenderPos(const std::shared_ptr<LivingEntity>& e)
{
	if (auto ice = std::dynamic_pointer_cast<IClientEntity>(e)) {
		if (ice->hasRenderPos) return ice->renderPos;
	}
	return e->getPositionD();
}

// Append the held-item mesh for one entity if it's drawable and holds
// something. Returns 1 if a slot was used.
int appendForOneEntity(std::vector<float>& cpuBuffer, const TextureManager* texMgr,
                       const LivingEntity& e, const glm::dvec3& worldPos,
                       const glm::dvec3& eyePos)
{
	if (!e.DoDraw() || e.heldItemType == 0) return 0;

	const float yawRad   = glm::radians(e.yaw);
	const float armAngle = armAngleFromAnimation(asCharacter(e));
	const glm::vec3 anchor = itemAnchorRel(worldPos, eyePos, yawRad, armAngle);

	return appendItemMesh(cpuBuffer, texMgr, e.heldItemType, anchor,
	                      yawRad, armAngle, CUBE_SCALE, SPRITE_SCALE)
	       ? 1 : 0;
}

} // namespace

HeldItemRenderer::HeldItemRenderer(const TextureManager* texMgr)
	: textureManager(texMgr)
{
	shader = std::make_unique<Shader>("shaders/cubePropShader.vert", "shaders/cubePropShader.frag");
	cpuBuffer.reserve(MAX_ITEMS * FLOATS_PER_ITEM);
	initGL();
}

HeldItemRenderer::~HeldItemRenderer()
{
	if (glfwGetCurrentContext()) {
		if (VBO) glDeleteBuffers(1, &VBO);
		if (VAO) glDeleteVertexArrays(1, &VAO);
	}
	VBO = VAO = 0;
}

void HeldItemRenderer::initGL()
{
	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);

	glBindVertexArray(VAO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);

	glBufferData(GL_ARRAY_BUFFER,
	             MAX_ITEMS * FLOATS_PER_ITEM * sizeof(float),
	             nullptr, GL_DYNAMIC_DRAW);

	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, STRIDE * sizeof(float),
	                      reinterpret_cast<void*>(0));
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, STRIDE * sizeof(float),
	                      reinterpret_cast<void*>(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, STRIDE * sizeof(float),
	                      reinterpret_cast<void*>(5 * sizeof(float)));
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);
}

void HeldItemRenderer::drawForEntities(const glm::mat4& projection, const glm::mat4& view,
                                       const glm::dvec3& eyePos,
                                       const std::vector<std::shared_ptr<LivingEntity>>& entities,
                                       LivingEntity* localPlayer)
{
	cpuBuffer.clear();
	int itemCount = 0;

	// Local player isn't in renderer->livingEntities — feed it in here so its
	// hand renders in third-person.
	if (localPlayer && itemCount < MAX_ITEMS) {
		glm::dvec3 worldPos = localPlayer->getPositionD();
		if (auto* ice = dynamic_cast<IClientEntity*>(localPlayer)) {
			if (ice->hasRenderPos) worldPos = ice->renderPos;
		}
		itemCount += appendForOneEntity(cpuBuffer, textureManager,
		                                *localPlayer, worldPos, eyePos);
	}

	for (const auto& e : entities) {
		if (!e) continue;
		if (itemCount >= MAX_ITEMS) break;
		itemCount += appendForOneEntity(cpuBuffer, textureManager,
		                                *e, entityRenderPos(e), eyePos);
	}

	if (itemCount == 0) return;

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0,
	                static_cast<GLsizeiptr>(cpuBuffer.size() * sizeof(float)),
	                cpuBuffer.data());

	shader->use();
	glBindVertexArray(VAO);
	if (textureManager) {
		textureManager->bind(GL_TEXTURE0);
		shader->setInt("blockTextures", 0);
	}

	glm::mat4 viewRot = view;
	viewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	shader->setMat4("projection", projection);
	shader->setMat4("viewRot", viewRot);

	glDisable(GL_CULL_FACE);
	glDrawArrays(GL_TRIANGLES, 0, itemCount * VERTS_PER_ITEM);
	glEnable(GL_CULL_FACE);
}

void HeldItemRenderer::drawFirstPerson(const glm::mat4& projection, uint16_t heldItemType,
                                       const Character* localCharacter)
{
	if (heldItemType == 0) return;

	cpuBuffer.clear();

	// Animation-driven offsets in camera space. The camera follows the player
	// in 1P so we don't need a yaw rotation; instead we animate via:
	//   • armAngle: punch + walk-arm-sway, rotates the cube around camera-X
	//     (screen right axis) so the swing arcs through the bottom-right corner
	//   • viewmodel bob/sway: vertical/horizontal sinusoidal offset tied to
	//     walkPhase, so the item bobs while you move
	const float armAngle = VM_SWING_GAIN * armAngleFromAnimation(localCharacter);

	float bobY = 0.0f, bobX = 0.0f;
	if (localCharacter && localCharacter->characterBodyParts.onWalkAnimation) {
		// walkPhase ∈ [0,1). Bob twice per stride (legs alternate), sway once.
		const float p = localCharacter->characterBodyParts.walkPhase;
		bobY = std::abs(std::sin(p * 2.0f * static_cast<float>(M_PI))) * VM_BOB_GAIN_Y;
		bobX = std::sin(p * 2.0f * static_cast<float>(M_PI)) * VM_BOB_GAIN_X;
	}

	const glm::vec3 anchorRel(VM_HAND_X + bobX, VM_HAND_Y - bobY, VM_HAND_Z);

	ItemType type = itemIDToItemType(heldItemType);
	const std::size_t start = cpuBuffer.size();

	bool drawnSomething = false;
	if (isItemFlat(type)) {
		const int layer = textureManager ? textureManager->getItemSpriteLayer(type) : 0;
		if (auto* b = std::get_if<BlockType>(&type)) {
			(void)b;
			buildItemSprite(cpuBuffer, 0.0f, 0.0f, 0.0f, layer);
			const float s = VM_SPRITE / 0.25f;
			transformVerts(cpuBuffer, start, s,
			               anchorRel.x,
			               anchorRel.y - 0.5f * VM_SPRITE,
			               anchorRel.z);
		} else {
			buildItemBillboard(cpuBuffer, glm::dvec3(anchorRel), layer);
			const float s = VM_SPRITE / 0.25f;
			transformVerts(cpuBuffer, start, s,
			               anchorRel.x * (1.0f - s),
			               anchorRel.y * (1.0f - s) - 0.5f * VM_SPRITE,
			               anchorRel.z * (1.0f - s));
		}
		drawnSomething = true;
	} else if (auto* b = std::get_if<BlockType>(&type)) {
		buildCube(cpuBuffer, 0.0f, 0.0f, 0.0f, 0, 0, *b, textureManager);
		// Camera-space matrix. Order: translate to hand → arm swing around
		// camera-X (screen right) → small rest tilts for a 3/4 readable view
		// → scale → center.
		glm::mat4 M(1.0f);
		M = glm::translate(M, anchorRel);
		M = glm::rotate(M, -armAngle,         glm::vec3(1.0f, 0.0f, 0.0f));
		M = glm::rotate(M, -VM_REST_TILT_X,   glm::vec3(1.0f, 0.0f, 0.0f));
		M = glm::rotate(M,  VM_REST_TILT_Y,   glm::vec3(0.0f, 1.0f, 0.0f));
		M = glm::scale(M, glm::vec3(VM_CUBE));
		M = glm::translate(M, glm::vec3(-0.5f));
		transformVertsMat(cpuBuffer, start, M);
		drawnSomething = true;
	}

	if (!drawnSomething) return;

	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferSubData(GL_ARRAY_BUFFER, 0,
	                static_cast<GLsizeiptr>(cpuBuffer.size() * sizeof(float)),
	                cpuBuffer.data());

	shader->use();
	glBindVertexArray(VAO);
	if (textureManager) {
		textureManager->bind(GL_TEXTURE0);
		shader->setInt("blockTextures", 0);
	}

	shader->setMat4("projection", projection);
	shader->setMat4("viewRot", glm::mat4(1.0f));

	// Always-on-top: clear depth (force mask on first — clouds composite may
	// have left it disabled), then keep depth test enabled for cube self-sort.
	glDepthMask(GL_TRUE);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDrawArrays(GL_TRIANGLES, 0, VERTS_PER_ITEM);
	glEnable(GL_CULL_FACE);
}
