
#ifndef HELD_ITEM_RENDERER_HPP
#define HELD_ITEM_RENDERER_HPP

#include <memory>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>
#include <glad/glad.h>

#include "Shader.hpp"
#include "LivingEntity.hpp"

class TextureManager;
class Character;

// Renders a single item (block cube or 2D sprite) at each visible player's
// right hand. Reuses the cubePropShader pipeline used by dropped items so the
// same vertex format, texture array, and lighting path apply. Two entry
// points:
//   - drawForEntities: third-person + every remote player, batched into one
//     draw call. Vertices emitted in camera-relative space.
//   - drawFirstPerson: local player's hand viewmodel, in camera space (the
//     view matrix is replaced with identity so the cube stays glued to the
//     screen).
//
// `heldItemType == 0` (or AIR) skips rendering for that entity.
class HeldItemRenderer {
	static constexpr int FLOATS_PER_VERT = 6;   // pos(3) + uv(2) + texLayer(1)
	static constexpr int VERTS_PER_ITEM  = 36;  // matches buildCube / padded sprite
	static constexpr int FLOATS_PER_ITEM = VERTS_PER_ITEM * FLOATS_PER_VERT;
	static constexpr int MAX_ITEMS       = 64;  // plenty for visible players

	GLuint VAO = 0;
	GLuint VBO = 0;
	// Separate VAO/VBO for the 1P weapon
	GLuint weaponVAO = 0;
	GLuint weaponVBO = 0;
	GLsizei weaponVBOCapacityBytes = 0;
	std::unordered_map<int, std::vector<float>> weaponMeshCache;
	std::unique_ptr<Shader> shader;
	const TextureManager* textureManager = nullptr;
	std::vector<float> cpuBuffer; // staging: MAX_ITEMS * FLOATS_PER_ITEM

public:
	explicit HeldItemRenderer(const TextureManager* texMgr);
	~HeldItemRenderer();

	// One batched draw for every visible LivingEntity holding something.
	// `localPlayer` is processed alongside `entities` so the local player's
	// hand renders in third-person — it's tracked by livingEntitiesManager
	// but not present in `renderer->livingEntities`. Pass nullptr to skip.
	// Skips entities with DoDraw()==false (first-person mode hides the
	// local mesh, so the viewmodel takes over via drawFirstPerson) and any
	// entity whose heldItemType is 0/AIR.
	void drawForEntities(const glm::mat4& projection, const glm::mat4& view,
	                     const glm::dvec3& eyePos,
	                     const std::vector<std::shared_ptr<LivingEntity>>& entities,
	                     LivingEntity* localPlayer = nullptr);

	// Viewmodel for the local player. heldItemType==0 short-circuits.
	// `localCharacter` lets the viewmodel animate during arm swings — pass
	// the same ClientPlayer used elsewhere; nullptr keeps the cube static.
	void drawFirstPerson(const glm::mat4& projection, const glm::mat4& view,
	                     uint16_t heldItemType,
	                     const Character* localCharacter = nullptr);

	// Same uniforms App.cpp uploads on the other entity shaders go here too.
	Shader& getShader() { return *shader; }

	// Debug-only:
	// `clearWeaponMeshCache()` after editing `voxelDepth` so the cached
	// extrusion gets rebuilt with the new thickness.
	struct WeaponTuning {
		float* leanDeg;
		float* depthDeg;
		float* size;
		float* hiltDX;
		float* hiltDY;
		float* hiltDZ;
		float* voxelDepth;
		float* handX;
		float* handY;
		float* handZ;
	};
	static WeaponTuning getWeaponTuning();
	void clearWeaponMeshCache() { weaponMeshCache.clear(); }

private:
	void initGL();
};

#endif
