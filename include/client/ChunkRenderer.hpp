
#ifndef CHUNK_RENDERER_HPP
#define CHUNK_RENDERER_HPP

#include <glad/glad.h>
#include <cstring>
#include <vector>

#include "GLFW/glfw3.h"
#include <glm/glm.hpp>
#include "Chunk.hpp"
#include "TextureManager.hpp"
#include "blockRenderingHelperFunctions.hpp"
#include "VegetationRenderer.hpp"
#include "PackedVertex.hpp"
#include "TerrainGPUBuffer.hpp"
#include <memory>

#ifdef _WIN32
typedef unsigned int uint;
#endif

class ChunkRenderer : public Chunk {

    // ── Terrain GPU slot ──────────────────────────────────────────────────────
    // Terrain vertices live in the global TerrainGPUBuffer (SSBO).
    // gpuSlot is the ChunkInfo slot index; INVALID_SLOT means not yet uploaded.
    uint32_t gpuSlot          = TerrainGPUBuffer::INVALID_SLOT;
    uint32_t terrainVtxFirst  = 0;  // starting vertex index in the SSBO
    uint32_t terrainVtxCount  = 0;  // vertex count (0 if empty chunk)
    std::vector<PackedVertex> meshVertices;  // CPU-side, cleared after upload

	GLuint waterVAO = 0;
	GLuint waterVBO = 0;
	uint waterMeshVertexCount = 0;
	std::vector<PackedVertex> waterMeshVertices;

	const TextureManager* textureManager = nullptr;

	std::unique_ptr<VegetationRenderer> vegetationRenderer;

    void addFace(int x, int y, int z, BlockType type, int face, float skyLightLevel); // Add a face to the mesh vertices (solid blocks)
	void addWaterFace(int x, int y, int z, int face, float skyLightLevel); // Add a face to water mesh

	public:

		ChunkRenderer(std::istream& in);
		~ChunkRenderer();

		bool needsUpdate = false;
		bool neighbourNeedUpdate[4] { false };

		glm::vec3 cachedMinP;
		glm::vec3 cachedMaxP;
		const glm::vec3& getCachedMinP() const { return cachedMinP; }
		const glm::vec3& getCachedMaxP() const { return cachedMaxP; }

		// Set the TextureManager (must be called before building meshes)
		void setTextureManager(const TextureManager* tm) { textureManager = tm; }

		// Release GL resources
		void releaseGL();

		void updateMesh();
		void buildMesh(); // Build both solid and water meshes
		void buildMeshData();
		void uploadMesh();
		void buildVegetationMesh() const; // Build vegetation instanced mesh

		// SSBO-based terrain (no per-chunk VAO/VBO for solid geometry).
		inline uint32_t getGpuSlot()         const { return gpuSlot; }
		inline uint32_t getTerrainVtxFirst() const { return terrainVtxFirst; }
		inline uint32_t getTerrainVtxCount() const { return terrainVtxCount; }
		// Alias used by debug stats / frustum radar.
		inline uint getMeshVertexCount() const { return (uint)terrainVtxCount; }

		// Water still uses a per-chunk VAO (water is not MDI yet).
		inline const GLuint getWaterVao() const {return waterVAO;}
		inline const uint getWaterMeshVertexCount() const {return waterMeshVertexCount;}

		inline VegetationRenderer* getVegetationRenderer() const { return vegetationRenderer.get(); }

		// Leaf-rendering strategy. Drives both mesh emission and fragment-shader
		// alpha test. Switching modes only affects the *next* mesh rebuild; for
		// instant effect call buildMesh() on every loaded chunk (or press F3+A).
		//
		//   Fast   — leaves treated as opaque blocks. Mesher culls leaf-to-leaf
		//            and solid-to-leaf faces. Fragment shader skips alpha test.
		//            Cheapest; leaves look like solid green cubes.
		//   Fancy  — leaves treated as transparent (original behaviour). Every
		//            face emitted, including leaves seen through other leaves.
		//            Most expensive; visually richest.
		//   Smart  — leaves alpha-tested (cutouts visible on outer faces) but
		//            mesher culls like Fast. Outer surface looks like leaves;
		//            inside each canopy is hollow.
		enum class LeafRenderMode { Fast, Fancy, Smart };
		static LeafRenderMode sLeafRenderMode;
};

#endif
