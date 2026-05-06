
#ifndef RENDERER_HPP
#define RENDERER_HPP

#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <chrono>
#include <future>

#include <glm/glm.hpp>
#include <memory>
#include <zstd.h>
#include <optional>

#include "Shader.hpp"
#include "ChunkRenderer.hpp"
#include "Protocol.hpp"
#include "CommonWorld.hpp"

#include "ItemPropEntity.hpp"
#include "LivingEntitiesManager.hpp"
#include "ClientPlayer.hpp"
#include "ClientCreeper.hpp"
#include "ClientZombie.hpp"
#include "Frustum.hpp"

#ifdef _WIN32
typedef unsigned int uint;
#endif

class TextureManager;

// previously half of World

struct chunkData {
	uint32_t compressedSize;
	uint32_t uncompressedSize;
	std::vector<uint8_t> chunkBuffer;
	// size_t receivedBytes = 0;
}; 

class Renderer final : public CommonWorld<ChunkRenderer> {
	std::unordered_map<ChunkPos, chunkData> chunksData; //building chunk
	std::unordered_set<ChunkPos> chunksToBuild; // chunk to build and upload mesh
	// When each chunk's last fragment arrived. Used to give newly-received
	// chunks a grace period before organizeChunks is allowed to evict them
	// by distance — otherwise a stale player position right after a teleport
	// or respawn can erase chunks that JUST arrived, and the server (which
	// already marked them sent in PlayerKnownChunks) never re-sends them.
	std::unordered_map<ChunkPos, std::chrono::steady_clock::time_point> chunkReceiveTime;
	std::vector<std::weak_ptr<ChunkRenderer>> renderedChunks;
	// Cached visibility list from the most recent terrain pass — reused by
	// renderVegetationOnly so we don't re-run frustum culling. Mutable because
	// render() is const-qualified.
	mutable std::vector<std::shared_ptr<ChunkRenderer>> m_lastVisibleChunks;
	float maxRenderedChunkDist = 0.0f; // world-space distance to edge of farthest rendered chunk
	Frustum cameraFrustum;
  glm::dvec3 frustumEyePos = glm::dvec3(0.0);
	bool frustumCullingEnabled = true;
	float maxRenderDistanceOverride = 0.0f; // 0 = no cap; set by water reflection pass
	float vegetationMaxDistance = 0.0f;     // 0 = no cap; cull distant vegetation chunks
	int   vegetationSwayQuality = 2;        // 0 none, 1 low (1 sin), 2 high (current)
	float vegetationSwayMaxDistance = 0.0f; // 0 = no fade; world-space LOD cutoff
	int   vegetationDensity = 1;            // 1 = all instances; N = render every Nth
	const TextureManager* textureManager = nullptr;
	std::shared_ptr<Shader> vegetationShader = nullptr;

	void linkNeighbors(int chunkX, int chunkZ, std::shared_ptr<ChunkRenderer> &chunk);
	std::unordered_map<ItemID, std::weak_ptr<Entity>> entitiesMap; //fast lookup

	mutable size_t m_drawCallCount = 0; // for debug stats

	public:
		std::vector<std::weak_ptr<ChunkRenderer>> getRenderedChunks();

     /// Update the camera frustum for culling. The view-projection should be
		/// built from a translation-free view matrix (camera at origin), and eyePos
		/// is used to test world AABBs in camera-relative space.
		void updateFrustum(const glm::mat4& viewProjection, const glm::dvec3& eyePos) {
			cameraFrustum.update(viewProjection);
			frustumEyePos = eyePos;
		}

		/// Get the current camera frustum (for debug visualization).
		const Frustum& getFrustum() const { return cameraFrustum; }

		bool isFrustumCullingEnabled() const { return frustumCullingEnabled; }
		void setFrustumCullingEnabled(bool enabled) { frustumCullingEnabled = enabled; }

		// Optional per-frame override that caps the chunk render distance during
		// the next render() call (used by water reflection to avoid drawing
		// distant terrain into a tiny offscreen target). 0 = no cap.
		float getMaxRenderDistanceOverride() const { return maxRenderDistanceOverride; }
		void  setMaxRenderDistanceOverride(float d) { maxRenderDistanceOverride = d; }

		// Cull vegetation in chunks whose center is farther than this from the
		// camera (in world units). 0 = no cap. Cheap CPU-side filter; saves
		// instanced vegetation draw calls in dense biomes.
		float getVegetationMaxDistance() const { return vegetationMaxDistance; }
		void  setVegetationMaxDistance(float d) { vegetationMaxDistance = d; }

		// Wind sway shader cost — uniform-controlled branch in vegetation.vert.
		//   0 = none (skip all sin/cos sway math), 1 = low (1 sin), 2 = high (current 3-5 sin)
		int  getVegetationSwayQuality() const { return vegetationSwayQuality; }
		void setVegetationSwayQuality(int q)  { vegetationSwayQuality = (q < 0 ? 0 : (q > 2 ? 2 : q)); }

		// World-space distance beyond which sway fades to zero (cheaper LOD
		// for distant vegetation while still showing it). 0 = no fade.
		float getVegetationSwayMaxDistance() const { return vegetationSwayMaxDistance; }
		void  setVegetationSwayMaxDistance(float d) { vegetationSwayMaxDistance = d; }

		// Render only every Nth vegetation instance. 1 = full density, 2 =
		// half (every other), 3 = third, etc.
		int  getVegetationDensity() const { return vegetationDensity; }
		void setVegetationDensity(int n)  { vegetationDensity = (n < 1 ? 1 : n); }

		void setTextureManager(const TextureManager* tm) { textureManager = tm; }
		void setVegetationShader(const std::shared_ptr<Shader>& shader) { vegetationShader = shader; }
		const std::shared_ptr<Shader>& getVegetationShader() const { return vegetationShader; }

		/// Draw a top-down ImGui radar showing which chunks pass frustum culling.
		void drawFrustumCullingDebug(const glm::vec3& cameraPos, const glm::vec3& cameraFront,
		                             float fovDeg, float aspectRatio, float nearP, float farP);

		void processMeshUpdates();
		void render(const std::shared_ptr<Shader> &shaderProgram,
		            const glm::mat4& view,
		            const glm::dvec3& eyePos,
		            bool renderVegetation = true) const ;

		/// Render only the terrain chunk loop (no vegetation). Populates the
		/// internal visibility cache so a subsequent `renderVegetationOnly`
		/// call can reuse the same chunk list — used by the Z-prepass path,
		/// where terrain runs twice (prepass + color) and vegetation once.
		void renderTerrainOnly(const std::shared_ptr<Shader>& shaderProgram,
		                       const glm::mat4& view,
		                       const glm::dvec3& eyePos) const;

		/// Render only the vegetation pass, reusing the visible-chunks cache
		/// from the most recent `render()` / `renderTerrainOnly()` call.
		void renderVegetationOnly(const glm::mat4& view, const glm::dvec3& eyePos) const;

		/// Update vegetation shader uniforms (for reflection pass where view/clip differ from main camera)
		void updateVegetationUniforms(const glm::mat4& view, const glm::mat4& projection,
		                              const glm::vec4& clipPlane, const glm::vec3& viewPos) const;

		/// Render only chunks visible inside a light-space ortho frustum (for CSM shadow passes).
       void renderShadow(const std::shared_ptr<Shader> &shaderProgram, const glm::mat4 &lightSpaceMatrix,
						 const glm::dvec3& eyePos) const;
       void renderWater(const std::shared_ptr<Shader>& shaderProgram, const glm::dvec3& eyePos) const;

		/// Returns true if any water chunk is visible in the current frustum.
		bool hasVisibleWater() const;

		void buildChunks();
		void updateChunk(const NetModifiedBlockData &pkt);
		void organizeChunks(const std::pair<int, int> pos, int loadRadius, float deltaTime = 0.016f);
    	void draw(const std::shared_ptr<Shader> &shaderProgram, const GLuint &VAO, const uint &vertexCount) const; // Draw the chunk using the given shader program

		void prepareChunk(const NetChunkHeader& pkt);
		void receiveChunk(const NetChunkData& pkt);

		bool setBlockWorld(glm::ivec3 globalCoords, std::optional<glm::ivec3> faceNormal, BlockType type) override;

		inline size_t getVisibleChunkCount() const {
			return renderedChunks.size();
		}

		/// Returns the world-space distance from the camera to the edge of the
		/// farthest currently-rendered chunk.
		float getMaxRenderedChunkDist() const { return maxRenderedChunkDist; }

		LivingEntitiesManager livingEntitiesManager;
		void onEntity(NetEntityMove &pkt, double serverTime);	// handles NetEntityMove packet
     void drawCharacters(const glm::mat4 &projection, const glm::mat4 &view,
						  const glm::dvec3& eyePos, const float deltatime);

		size_t getDrawCallCount() const { return m_drawCallCount; }
		void resetDrawCallCount() { m_drawCallCount = 0; }
};

#endif