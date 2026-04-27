
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
	float maxRenderedChunkDist = 0.0f; // world-space distance to edge of farthest rendered chunk
	Frustum cameraFrustum;
  glm::dvec3 frustumEyePos = glm::dvec3(0.0);
	bool frustumCullingEnabled = true;
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

		/// Update vegetation shader uniforms (for reflection pass where view/clip differ from main camera)
		void updateVegetationUniforms(const glm::mat4& view, const glm::mat4& projection,
		                              const glm::vec4& clipPlane, const glm::vec3& viewPos) const;

		/// Render only chunks visible inside a light-space ortho frustum (for CSM shadow passes).
       void renderShadow(const std::shared_ptr<Shader> &shaderProgram, const glm::mat4 &lightSpaceMatrix,
						 const glm::dvec3& eyePos) const;
		void renderWater() const;

		/// Returns true if any water chunk is visible in the current frustum.
		bool hasVisibleWater() const;

		void buildChunks();
		void updateChunk(const NetModifiedBlockData &pkt);
		void organizeChunks(const std::pair<int, int> pos, int loadRadius, float deltaTime = 0.016f);
    	void draw(const std::shared_ptr<Shader> &shaderProgram, const GLuint &VAO, const uint &meshVerticesSize) const; // Draw the chunk using the given shader program

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