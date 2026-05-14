
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

#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace {

// Per-vertex stride in floats (pos.xyz, uv.xy, texLayer) — must match the
// cubePropShader vertex layout and buildCube/buildItemSprite output.
constexpr int STRIDE = 6;

// Hand anchor in player-local coords (X=forward, Y=up, Z=right).
//   - SHOULDER_HEIGHT, SHOULDER_RIGHT: shoulder pivot, derived from the skin
//     model in Character::setPartsDimensions(): 24/31 × entityHeight up, and
//     6 skin-Z units out
//   - ARM_LENGTH: shoulder→hand distance = 12 skin-Y units = 12/31 × 1.8.
//     A rest arm-pose value lets the rendered item project slightly forward of
//     the body even when the character isn't animating
constexpr float SHOULDER_HEIGHT = 1.394f;
constexpr float SHOULDER_RIGHT  = 0.348f;
constexpr float ARM_LENGTH      = 0.697f;
constexpr float ARM_REST_ANGLE  = 0.45f; // ~26° forward — visible held pose at rest
// buildCube(isIlluminated=false) emits a 0.2-unit cube centered at (0, 0.1, 0);
// these scales are applied straight to that source size — final cube size in
// world units is CUBE_SOURCE_SIZE * CUBE_SCALE.
constexpr float CUBE_SOURCE_SIZE = 0.2f;
constexpr float CUBE_Y_CENTER    = 0.1f; // half-height: source verts span y in [0, 0.2]
constexpr float CUBE_SCALE      = 1.6f;  // → ~0.32-unit cube held by entity
constexpr float SPRITE_SCALE    = 0.42f;
// Cube-local "presentation" tilts for third-person, so the held cube shows
// three faces (top + two sides) rather than a flat single face.
constexpr float TP_REST_TILT_X  = 0.30f;
constexpr float TP_REST_TILT_Y  = 0.55f;

// Weapons (swords etc.) get a "real object" pose instead of the camera-facing
// billboard the other 2D items use. The texture is still a flat 2D sprite, but
// it's placed on a single fixed-orientation quad — third-person: blade
// extending from the hand along the forearm with the flat side in the
// saggital plane; first-person: a diagonal blade going from bottom-right
// hilt to upper-left tip
constexpr float TP_WEAPON_LENGTH = 0.65f; // shoulder→tip in world units
constexpr float TP_WEAPON_WIDTH  = 0.18f;
// NOTE: VM_WEAPON_* and VM_HAND_* are intentionally NOT `constexpr` — they
// are exposed through HeldItemRenderer::getWeaponTuning() so the ImGui debug
// window can tweak them at runtime. Once the pose is dialed in, switch them
// back to `constexpr` and remove the ImGui hooks.
float VM_WEAPON_SIZE      = 0.5f;
// Tilt forward/backward
float VM_WEAPON_LEAN_DEG  = 40.0f;
// Tilt the blade around its axis
float VM_WEAPON_DEPTH_DEG = -90.0f;
// Voxel-extrusion thickness in canonical units (the voxel grid spans
// [0,1]×[0,1] in width/length, so 1/16 = "one pixel deep" if the texture is
// 16×16).Changing this requires clearWeaponMeshCache()
float VM_WEAPON_VOXEL_DEPTH = 1.0f / 16.0f;
// Hilt anchor offset from VM_HAND_*: move weapon around on XYZ axis
float VM_WEAPON_HILT_DX = -0.558f;
float VM_WEAPON_HILT_DY = 0.531f;
float VM_WEAPON_HILT_DZ =  0.888f;

// First-person viewmodel placement in camera space (X right, Y up, -Z forward).
float VM_HAND_X    =  0.95f;
float VM_HAND_Y    = -0.65f;
float VM_HAND_Z    = -0.90f;
constexpr float VM_CUBE      =  3.5f;  // → ~0.7-unit cube in camera space
constexpr float VM_SPRITE    =  0.65f;
constexpr float VM_REST_TILT_X = 0.40f; // small downward tilt at rest
constexpr float VM_REST_TILT_Y = 0.40f; // and around Y so we see a 3/4 view
constexpr float VM_SWING_GAIN  = 0.50f; // dampens arm angle so it stays on screen
constexpr float VM_BOB_GAIN_Y  = 0.06f; // 1P walking bob, vertical (camera-space)
constexpr float VM_BOB_GAIN_X  = 0.04f; // 1P walking sway, horizontal

// Separate shoulder + elbow rotations for the right arm at the current frame.
// We need both — collapsing them into one angle on a straight arm makes the
// hand overshoot vertically during a punch (the bent forearm has a much
// shorter "up" reach than a 90°+30° straight-arm rotation would have).
struct ArmPose { float shoulder; float elbow; };

ArmPose armPoseFromAnimation(const Character* ch)
{
	if (!ch) return {0.0f, 0.0f};
	if (ch->characterBodyParts.onArmSwingAnimation) {
		// Same constants as Character::swingArmAnimation. The two angles are
		// applied to different pivots (shoulder vs elbow), so two-segment
		// math is needed to find the actual hand position.
		const float t = ch->characterBodyParts.armSwingPhase;
		const float curve = std::sin(t * static_cast<float>(M_PI));
		return { curve * 1.6f, curve * 0.6f };
	}
	if (ch->characterBodyParts.onWalkAnimation) {
		// Right arm in walkAnimation: rotateBodyPart(rightArm, pivot, -angle)
		// → effective shoulder rotation = (-angle) * 0.8. The walking forearm
		// bend is minor and only fires on back-swing, so we ignore it for the
		// held-item anchor.
		const float a = std::sin(ch->characterBodyParts.walkPhase * 2.0f * static_cast<float>(M_PI));
		return { -a * 0.8f, 0.0f };
	}
	return {0.0f, 0.0f};
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

// Append a single flat textured quad in canonical pose (X = width, Y = length,
// Z = 0), centered on X in [-0.5, 0.5], stretched on Y in [0, 1]. The caller
// applies a model matrix to position/orient/scale it. UVs match the sprite
// convention: V=0 at the hilt (y=0), V=1 at the tip (y=1). Padded to the 36-
// vert slot the cubePropShader VBO allocates per item.
void buildWeaponQuad(std::vector<float>& buf, int texLayer)
{
	const float layer = static_cast<float>(texLayer);
	auto v = [&](float x, float y, float u, float vv) {
		buf.push_back(x); buf.push_back(y); buf.push_back(0.0f);
		buf.push_back(u); buf.push_back(vv); buf.push_back(layer);
	};
	// Two triangles: BL, BR, TR, TR, TL, BL.
	v(-0.5f, 0.0f, 0.0f, 0.0f);
	v( 0.5f, 0.0f, 1.0f, 0.0f);
	v( 0.5f, 1.0f, 1.0f, 1.0f);
	v( 0.5f, 1.0f, 1.0f, 1.0f);
	v(-0.5f, 1.0f, 0.0f, 1.0f);
	v(-0.5f, 0.0f, 0.0f, 0.0f);
	// Degenerate padding so the GPU rejects the rest of the 36-vert slot.
	for (int i = 0; i < 30; ++i) v(-0.5f, 0.0f, 0.0f, 0.0f);
}

// Append a 3D slab (thin box) in canonical pose: X in [-0.5, 0.5] (width),
// Y in [0, 1] (length), Z in [-0.5, 0.5] (thickness). The FRONT and BACK
// faces carry the full sword texture. The four thin side/cap faces sample
// the texture ALONG ITS DIAGONAL — which is where the sword actually is in
// the source image (hilt at texture-BR = UV(1,0), tip at texture-TL = UV(0,1)).
// That way the sides read as a thin sword-colored edge running from hilt
// color to tip color, instead of a smeared rectangle of mostly transparent
// pixels. Face order matches FACE_NORMALS in cubePropShader.vert:
// +Z, -Z, +Y, -Y, +X, -X. 6 faces × 6 verts = exactly the 36-vert slot.
void buildItemSlab(std::vector<float>& buf, int texLayer)
{
	const float layer = static_cast<float>(texLayer);
	auto v = [&](float x, float y, float z, float u, float vv) {
		buf.push_back(x); buf.push_back(y); buf.push_back(z);
		buf.push_back(u); buf.push_back(vv); buf.push_back(layer);
	};

	// Texture's hilt corner (full opacity, hilt color) and tip corner.
	constexpr float HILT_U = 1.0f, HILT_V = 0.0f;
	constexpr float TIP_U  = 0.0f, TIP_V  = 1.0f;

	// FRONT (+Z) — full sword sprite, axis-aligned.
	v(-0.5f, 0.0f,  0.5f, 0.0f, 0.0f);
	v( 0.5f, 0.0f,  0.5f, 1.0f, 0.0f);
	v( 0.5f, 1.0f,  0.5f, 1.0f, 1.0f);
	v( 0.5f, 1.0f,  0.5f, 1.0f, 1.0f);
	v(-0.5f, 1.0f,  0.5f, 0.0f, 1.0f);
	v(-0.5f, 0.0f,  0.5f, 0.0f, 0.0f);
	// BACK (-Z) — full sword sprite, mirrored in X so it's not reversed.
	v( 0.5f, 0.0f, -0.5f, 0.0f, 0.0f);
	v(-0.5f, 0.0f, -0.5f, 1.0f, 0.0f);
	v(-0.5f, 1.0f, -0.5f, 1.0f, 1.0f);
	v(-0.5f, 1.0f, -0.5f, 1.0f, 1.0f);
	v( 0.5f, 1.0f, -0.5f, 0.0f, 1.0f);
	v( 0.5f, 0.0f, -0.5f, 0.0f, 0.0f);
	// TOP cap (+Y) — every vertex samples the texture's tip pixel.
	v(-0.5f, 1.0f,  0.5f, TIP_U, TIP_V);
	v( 0.5f, 1.0f,  0.5f, TIP_U, TIP_V);
	v( 0.5f, 1.0f, -0.5f, TIP_U, TIP_V);
	v( 0.5f, 1.0f, -0.5f, TIP_U, TIP_V);
	v(-0.5f, 1.0f, -0.5f, TIP_U, TIP_V);
	v(-0.5f, 1.0f,  0.5f, TIP_U, TIP_V);
	// BOTTOM cap (-Y) — every vertex samples the hilt pixel.
	v(-0.5f, 0.0f, -0.5f, HILT_U, HILT_V);
	v( 0.5f, 0.0f, -0.5f, HILT_U, HILT_V);
	v( 0.5f, 0.0f,  0.5f, HILT_U, HILT_V);
	v( 0.5f, 0.0f,  0.5f, HILT_U, HILT_V);
	v(-0.5f, 0.0f,  0.5f, HILT_U, HILT_V);
	v(-0.5f, 0.0f, -0.5f, HILT_U, HILT_V);
	// RIGHT (+X) — sample along the texture's diagonal. y=0 → hilt UV,
	// y=1 → tip UV. The GPU interpolates between them across the face, so
	// each horizontal band along the slab's length shows the pixel of the
	// sword at that fraction of its travel from hilt to tip.
	v( 0.5f, 0.0f, -0.5f, HILT_U, HILT_V);
	v( 0.5f, 0.0f,  0.5f, HILT_U, HILT_V);
	v( 0.5f, 1.0f,  0.5f, TIP_U,  TIP_V);
	v( 0.5f, 1.0f,  0.5f, TIP_U,  TIP_V);
	v( 0.5f, 1.0f, -0.5f, TIP_U,  TIP_V);
	v( 0.5f, 0.0f, -0.5f, HILT_U, HILT_V);
	// LEFT (-X) — same diagonal sampling.
	v(-0.5f, 0.0f,  0.5f, HILT_U, HILT_V);
	v(-0.5f, 0.0f, -0.5f, HILT_U, HILT_V);
	v(-0.5f, 1.0f, -0.5f, TIP_U,  TIP_V);
	v(-0.5f, 1.0f, -0.5f, TIP_U,  TIP_V);
	v(-0.5f, 1.0f,  0.5f, TIP_U,  TIP_V);
	v(-0.5f, 0.0f,  0.5f, HILT_U, HILT_V);
}

// Per-pixel-extruded weapon mesh
void buildWeaponVoxelMesh(std::vector<float>& buf,
                          const std::vector<unsigned char>& pixels,
                          int texSize, int texLayer, float thickness)
{
	if (pixels.empty() || texSize <= 0) return;

	auto opaqueAt = [&](int px, int py) -> bool {
		if (px < 0 || px >= texSize || py < 0 || py >= texSize) return false;
		const std::size_t idx = (static_cast<std::size_t>(py) * texSize + px) * 4 + 3;
		if (idx >= pixels.size()) return false;
		return pixels[idx] >= 26;
	};

	const float L = static_cast<float>(texLayer);
	const float cell = 1.0f / static_cast<float>(texSize);
	const float halfT = thickness * 0.5f;

	auto v = [&](float x, float y, float z, float u, float vv) {
		buf.push_back(x); buf.push_back(y); buf.push_back(z);
		buf.push_back(u); buf.push_back(vv); buf.push_back(L);
	};

	for (int py = 0; py < texSize; ++py) {
		for (int px = 0; px < texSize; ++px) {
			if (!opaqueAt(px, py)) continue;

			const float x0 =  px      * cell;
			const float x1 = (px + 1) * cell;
			const float y0 =  py      * cell;
			const float y1 = (py + 1) * cell;
			const float z0 = -halfT, z1 = halfT;

			// Every face of this voxel samples the same pixel center. We
			// pick the UV at the texel center to avoid bilinear bleed from
			// the (often transparent) neighbors at the cell boundary.
			const float u    = (px + 0.5f) * cell;
			const float vTex = (py + 0.5f) * cell;

			// FRONT (+Z) — always visible.
			v(x0, y0, z1, u, vTex); v(x1, y0, z1, u, vTex); v(x1, y1, z1, u, vTex);
			v(x1, y1, z1, u, vTex); v(x0, y1, z1, u, vTex); v(x0, y0, z1, u, vTex);
			// BACK (-Z) — always visible (slab is one voxel deep, no neighbors behind).
			v(x1, y0, z0, u, vTex); v(x0, y0, z0, u, vTex); v(x0, y1, z0, u, vTex);
			v(x0, y1, z0, u, vTex); v(x1, y1, z0, u, vTex); v(x1, y0, z0, u, vTex);
			// TOP (+Y) — emit only if no opaque neighbor in the +Y direction.
			if (!opaqueAt(px, py + 1)) {
				v(x0, y1, z1, u, vTex); v(x1, y1, z1, u, vTex); v(x1, y1, z0, u, vTex);
				v(x1, y1, z0, u, vTex); v(x0, y1, z0, u, vTex); v(x0, y1, z1, u, vTex);
			}
			// BOTTOM (-Y).
			if (!opaqueAt(px, py - 1)) {
				v(x0, y0, z0, u, vTex); v(x1, y0, z0, u, vTex); v(x1, y0, z1, u, vTex);
				v(x1, y0, z1, u, vTex); v(x0, y0, z1, u, vTex); v(x0, y0, z0, u, vTex);
			}
			// RIGHT (+X).
			if (!opaqueAt(px + 1, py)) {
				v(x1, y0, z0, u, vTex); v(x1, y0, z1, u, vTex); v(x1, y1, z1, u, vTex);
				v(x1, y1, z1, u, vTex); v(x1, y1, z0, u, vTex); v(x1, y0, z0, u, vTex);
			}
			// LEFT (-X).
			if (!opaqueAt(px - 1, py)) {
				v(x0, y0, z1, u, vTex); v(x0, y0, z0, u, vTex); v(x0, y1, z0, u, vTex);
				v(x0, y1, z0, u, vTex); v(x0, y1, z1, u, vTex); v(x0, y0, z1, u, vTex);
			}
		}
	}
}

// Compute the held-item anchor (cube center / sprite center) in camera-relative
// world space. The anchor tracks the actual hand at the END of the rendered
// two-segment arm: upper arm rotates around the shoulder by
// (ARM_REST_ANGLE + pose.shoulder), forearm rotates around the elbow by an
// additional pose.elbow. Each segment is half of ARM_LENGTH.
// pose comes from armPoseFromAnimation() and mirrors the visible arm.
glm::vec3 itemAnchorRel(const glm::dvec3& worldPos, const glm::dvec3& eyePos,
                        float yawRad, ArmPose pose)
{
	const float cy = std::cos(yawRad), sy = std::sin(yawRad);

	// Two-segment arm in the saggital plane (player-local fwd-up).
	//   upperAngle: rotation of the upper arm around the shoulder
	//   forearmAngle: rotation of the forearm in WORLD frame
	//                 (upper-arm rotation composes onto it)
	// Both segments are L/2; rotating (0, -L/2, 0) by angle around +Z gives
	// (sin(angle) * L/2, -cos(angle) * L/2, 0).
	const float upperAngle   = ARM_REST_ANGLE + pose.shoulder;
	const float forearmAngle = upperAngle + pose.elbow;
	const float halfArm      = ARM_LENGTH * 0.5f;
	const float fwdComp = (std::sin(upperAngle) + std::sin(forearmAngle)) * halfArm;
	const float upComp  = (-std::cos(upperAngle) - std::cos(forearmAngle)) * halfArm;

	// Shoulder in player-local coords: (0, SHOULDER_HEIGHT, SHOULDER_RIGHT).
	// Player-local → world: local x along world forward = (cos,0,sin); local z
	// along world right = (-sin,0,cos). Up = world up.
	const float localFwd   = fwdComp;
	const float localUp    = SHOULDER_HEIGHT + upComp;
	const float localRight = SHOULDER_RIGHT;

	const glm::dvec3 worldOffset(
		static_cast<double>(localFwd) * cy + static_cast<double>(localRight) * (-sy),
		static_cast<double>(localUp),
		static_cast<double>(localFwd) * sy + static_cast<double>(localRight) *  cy
	);

	return glm::vec3((worldPos - eyePos) + worldOffset);
}

// Build the model matrix for a held cube: translate to `anchorRel`, rotate
// player-local→world by yaw, apply forearm rotation around the player's right
// axis (cube is glued to the forearm, so it uses the FULL upper+elbow angle,
// not just the shoulder), apply cube-local tilts for a readable 3-face view,
// scale, and recenter.
// buildCube here outputs a 0.2-unit cube centered at (0, CUBE_Y_CENTER, 0),
// not a 0..1 cube — so the recenter step only nudges Y, never -0.5 on all axes.
glm::mat4 cubeModelMatrix(const glm::vec3& anchorRel, float yawRad,
                          ArmPose pose, float scale)
{
	const float forearmAngle = ARM_REST_ANGLE + pose.shoulder + pose.elbow;
	glm::mat4 M(1.0f);
	M = glm::translate(M, anchorRel);
	// In this engine yaw=π/2 faces world +Z; GLM's rotate is right-hand around
	// +Y, so we use the negative angle to match.
	M = glm::rotate(M, -yawRad, glm::vec3(0.0f, 1.0f, 0.0f));
	// Cube follows the forearm's world rotation around player-right.
	M = glm::rotate(M, forearmAngle, glm::vec3(0.0f, 0.0f, 1.0f));
	// Cube-local presentation tilts — applied in the cube's own frame so the
	// three-face view is preserved through every yaw / arm-swing pose.
	M = glm::rotate(M, TP_REST_TILT_Y, glm::vec3(0.0f, 1.0f, 0.0f));
	M = glm::rotate(M, TP_REST_TILT_X, glm::vec3(1.0f, 0.0f, 0.0f));
	M = glm::scale(M, glm::vec3(scale));
	M = glm::translate(M, glm::vec3(0.0f, -CUBE_Y_CENTER, 0.0f));
	return M;
}

// Emit the geometry for one held item into `buf` at the supplied anchor.
// Cubes get the full orient-to-player matrix; sprites/billboards stay
// self-oriented (X-cross is symmetric, billboard reorients per-frame).
bool appendItemMesh(std::vector<float>& buf, const TextureManager* texMgr,
                    uint16_t heldItemType, const glm::vec3& anchorRel,
                    float yawRad, ArmPose pose,
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
		} else if (isWeapon(type)) {
			// Real-object pose: a flat blade extending from the hand along the
			// forearm direction. Flat side lies in the player's saggital plane,
			// so side-on third-person views see the full silhouette.
			const float forearmAngle = ARM_REST_ANGLE + pose.shoulder + pose.elbow;
			const float sa = std::sin(forearmAngle), ca = std::cos(forearmAngle);
			// Player-local frame basis (X=forward, Y=up, Z=right):
			//   bladeY = forearm direction      = (sa, -ca, 0)
			//   bladeZ = lateral (saggital out) = (0, 0, 1)
			//   bladeX = bladeY × bladeZ        = (-ca, -sa, 0)
			const glm::vec3 bladeX(-ca, -sa, 0.0f);
			const glm::vec3 bladeY( sa, -ca, 0.0f);
			const glm::vec3 bladeZ(0.0f, 0.0f, 1.0f);
			glm::mat4 orient(1.0f);
			orient[0] = glm::vec4(bladeX, 0.0f);
			orient[1] = glm::vec4(bladeY, 0.0f);
			orient[2] = glm::vec4(bladeZ, 0.0f);

			buildWeaponQuad(buf, layer);
			glm::mat4 M(1.0f);
			M = glm::translate(M, anchorRel);
			M = glm::rotate(M, -yawRad, glm::vec3(0.0f, 1.0f, 0.0f));
			M = M * orient;
			M = glm::scale(M, glm::vec3(TP_WEAPON_WIDTH, TP_WEAPON_LENGTH, 1.0f));
			transformVertsMat(buf, start, M);
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
		                  cubeModelMatrix(anchorRel, yawRad, pose, cubeScale));
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

	const float yawRad     = glm::radians(e.yaw);
	const ArmPose pose     = armPoseFromAnimation(asCharacter(e));
	const glm::vec3 anchor = itemAnchorRel(worldPos, eyePos, yawRad, pose);

	return appendItemMesh(cpuBuffer, texMgr, e.heldItemType, anchor,
	                      yawRad, pose, CUBE_SCALE, SPRITE_SCALE)
	       ? 1 : 0;
}

} // namespace

HeldItemRenderer::WeaponTuning HeldItemRenderer::getWeaponTuning()
{
	return {
		&VM_WEAPON_LEAN_DEG,
		&VM_WEAPON_DEPTH_DEG,
		&VM_WEAPON_SIZE,
		&VM_WEAPON_HILT_DX,
		&VM_WEAPON_HILT_DY,
		&VM_WEAPON_HILT_DZ,
		&VM_WEAPON_VOXEL_DEPTH,
		&VM_HAND_X,
		&VM_HAND_Y,
		&VM_HAND_Z,
	};
}

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
		if (weaponVBO) glDeleteBuffers(1, &weaponVBO);
		if (weaponVAO) glDeleteVertexArrays(1, &weaponVAO);
	}
	VBO = VAO = 0;
	weaponVBO = weaponVAO = 0;
}

void HeldItemRenderer::initGL()
{
	auto setupAttribs = [](GLuint vao, GLuint vbo) {
		glBindVertexArray(vao);
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, STRIDE * sizeof(float),
		                      reinterpret_cast<void*>(0));
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, STRIDE * sizeof(float),
		                      reinterpret_cast<void*>(3 * sizeof(float)));
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, STRIDE * sizeof(float),
		                      reinterpret_cast<void*>(5 * sizeof(float)));
		glEnableVertexAttribArray(2);
	};

	glGenVertexArrays(1, &VAO);
	glGenBuffers(1, &VBO);
	glBindBuffer(GL_ARRAY_BUFFER, VBO);
	glBufferData(GL_ARRAY_BUFFER,
	             MAX_ITEMS * FLOATS_PER_ITEM * sizeof(float),
	             nullptr, GL_DYNAMIC_DRAW);
	setupAttribs(VAO, VBO);

	glGenVertexArrays(1, &weaponVAO);
	glGenBuffers(1, &weaponVBO);
	setupAttribs(weaponVAO, weaponVBO);
	weaponVBOCapacityBytes = 0;

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

void HeldItemRenderer::drawFirstPerson(const glm::mat4& projection,
                                       const glm::mat4& view,
                                       uint16_t heldItemType,
                                       const Character* localCharacter)
{
	if (heldItemType == 0) return;

	cpuBuffer.clear();

	// Animation-driven offsets in camera space. The camera follows the player
	// in 1P so we don't need a yaw rotation; instead we animate via:
	//   • armAngle: punch + walk-arm-sway, rotates the cube around camera-X
	//   • viewmodel bob/sway: vertical/horizontal sinusoidal offset tied to
	//     walkPhase, so the item bobs while you move
	const ArmPose pose = armPoseFromAnimation(localCharacter);
	const float armAngle = VM_SWING_GAIN * (pose.shoulder + pose.elbow);

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

	// Weapons get a dedicated per-pixel-extruded mesh path — a true 3D
	// sword-shaped voxel mesh instead of a slab-with-paint. The vert count is
	// variable (depends on the weapon's silhouette), so this path uses its
	// own VAO/VBO and bypasses the 36-vert/item slot entirely. Hand off the
	// whole frame to it and return.
	if (isItemFlat(type) && isWeapon(type) && textureManager) {
		const int layer = textureManager->getItemSpriteLayer(type);
		// Build & cache the canonical voxel mesh on first hold of this layer.
		auto cacheIt = weaponMeshCache.find(layer);
		if (cacheIt == weaponMeshCache.end()) {
			std::vector<float> mesh;
			buildWeaponVoxelMesh(mesh,
			                     textureManager->getLayerPixels(layer),
			                     textureManager->getTextureSize(),
			                     layer,
			                     VM_WEAPON_VOXEL_DEPTH);
			cacheIt = weaponMeshCache.emplace(layer, std::move(mesh)).first;
		}
		const std::vector<float>& canonical = cacheIt->second;
		if (canonical.empty()) return;

		// Build the camera-space model matrix, fold inverse(viewRot) into it
		// so the final mesh lands in camera-relative WORLD space (matches the
		// frame the lighting uniforms use — same trick the other 1P branches
		// apply with the per-vertex pass below).
		const glm::vec3 hilt = anchorRel
			+ glm::vec3(VM_WEAPON_HILT_DX, VM_WEAPON_HILT_DY, VM_WEAPON_HILT_DZ);
		glm::mat4 M(1.0f);
		M = glm::translate(M, hilt);
		M = glm::rotate(M, -armAngle,                          glm::vec3(1.0f, 0.0f, 0.0f));
		M = glm::rotate(M, glm::radians(VM_WEAPON_DEPTH_DEG),  glm::vec3(0.0f, 1.0f, 0.0f));
		M = glm::rotate(M, glm::radians(VM_WEAPON_LEAN_DEG),   glm::vec3(0.0f, 0.0f, 1.0f));
		M = glm::scale(M, glm::vec3(VM_WEAPON_SIZE));
		// Canonical voxel mesh has the hilt voxel at the X=1, Y=0 corner —
		// shift so it sits at the origin before any rotation, so rotations
		// pivot around the hilt point.
		M = glm::translate(M, glm::vec3(-1.0f, 0.0f, 0.0f));

		glm::mat4 viewRot = view;
		viewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		const glm::mat4 finalM = glm::inverse(viewRot) * M;

		// Stream the transformed mesh into the weaponVBO. Grow if needed.
		cpuBuffer.clear();
		cpuBuffer.reserve(canonical.size());
		for (std::size_t i = 0; i + 5 < canonical.size(); i += STRIDE) {
			const glm::vec4 p(canonical[i], canonical[i + 1], canonical[i + 2], 1.0f);
			const glm::vec4 q = finalM * p;
			cpuBuffer.push_back(q.x);
			cpuBuffer.push_back(q.y);
			cpuBuffer.push_back(q.z);
			cpuBuffer.push_back(canonical[i + 3]);
			cpuBuffer.push_back(canonical[i + 4]);
			cpuBuffer.push_back(canonical[i + 5]);
		}

		const GLsizeiptr bytesNeeded =
			static_cast<GLsizeiptr>(cpuBuffer.size() * sizeof(float));
		glBindBuffer(GL_ARRAY_BUFFER, weaponVBO);
		if (bytesNeeded > weaponVBOCapacityBytes) {
			glBufferData(GL_ARRAY_BUFFER, bytesNeeded, cpuBuffer.data(),
			             GL_DYNAMIC_DRAW);
			weaponVBOCapacityBytes = static_cast<GLsizei>(bytesNeeded);
		} else {
			glBufferSubData(GL_ARRAY_BUFFER, 0, bytesNeeded, cpuBuffer.data());
		}

		shader->use();
		glBindVertexArray(weaponVAO);
		textureManager->bind(GL_TEXTURE0);
		shader->setInt("blockTextures", 0);
		shader->setMat4("projection", projection);
		shader->setMat4("viewRot", viewRot);

		glDepthMask(GL_TRUE);
		glClear(GL_DEPTH_BUFFER_BIT);
		glEnable(GL_DEPTH_TEST);
		glDisable(GL_CULL_FACE);
		glDrawArrays(GL_TRIANGLES, 0,
		             static_cast<GLsizei>(cpuBuffer.size() / STRIDE));
		glEnable(GL_CULL_FACE);
		return;
	}

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
		// → scale → recenter Y (source cube has y in [0, 0.2]
		glm::mat4 M(1.0f);
		M = glm::translate(M, anchorRel);
		M = glm::rotate(M, -armAngle,         glm::vec3(1.0f, 0.0f, 0.0f));
		M = glm::rotate(M, -VM_REST_TILT_X,   glm::vec3(1.0f, 0.0f, 0.0f));
		M = glm::rotate(M,  VM_REST_TILT_Y,   glm::vec3(0.0f, 1.0f, 0.0f));
		M = glm::scale(M, glm::vec3(VM_CUBE));
		M = glm::translate(M, glm::vec3(0.0f, -CUBE_Y_CENTER, 0.0f));
		transformVertsMat(cpuBuffer, start, M);
		drawnSomething = true;
	}

	if (!drawnSomething) return;

	glm::mat4 viewRot = view;
	viewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const glm::mat4 invViewRot = glm::inverse(viewRot);
	for (std::size_t i = 0; i + 2 < cpuBuffer.size(); i += STRIDE) {
		const glm::vec4 p(cpuBuffer[i + 0], cpuBuffer[i + 1], cpuBuffer[i + 2], 1.0f);
		const glm::vec4 q = invViewRot * p;
		cpuBuffer[i + 0] = q.x;
		cpuBuffer[i + 1] = q.y;
		cpuBuffer[i + 2] = q.z;
	}

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
	shader->setMat4("viewRot", viewRot);

	// Always-on-top: clear depth (force mask on first — clouds composite may
	// have left it disabled), then keep depth test enabled for cube self-sort.
	glDepthMask(GL_TRUE);
	glClear(GL_DEPTH_BUFFER_BIT);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glDrawArrays(GL_TRIANGLES, 0, VERTS_PER_ITEM);
	glEnable(GL_CULL_FACE);
}
