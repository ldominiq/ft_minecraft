
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
#include <memory>

#ifdef _WIN32
typedef unsigned int uint;
#endif

class ChunkRenderer : public Chunk {

    GLuint VAO = 0;
    GLuint VBO = 0;
	uint meshVertexCount = 0;                      // number of PackedVertex entries (NOT bytes / floats)
    std::vector<PackedVertex> meshVertices;        // 8 bytes per vertex (was 11 floats)

	GLuint waterVAO = 0;
	GLuint waterVBO = 0;
	uint waterMeshVertexCount = 0;
	std::vector<PackedVertex> waterMeshVertices;

	GLuint placedWaterVAO = 0;
	GLuint placedWaterVBO = 0;
	uint placedWaterMeshVertexCount = 0;
	std::vector<PackedVertex> placedWaterMeshVertices;

public:
	struct TorchInstance { glm::ivec3 worldPos; BlockType variant; };
private:

	std::vector<TorchInstance> torchInstances;

	const TextureManager* textureManager = nullptr;

	std::unique_ptr<VegetationRenderer> vegetationRenderer;

    void addFace(int x, int y, int z, BlockType type, int face, float skyLightLevel,
                 bool waterAbove = false, float blockLightLevel = 0.0f); // Add a face to the mesh vertices (solid blocks)
	// Push a water face into `out` (caller picks ocean vs placed bucket).
	void addWaterFace(int x, int y, int z, int face, float skyLightLevel,
	                  std::vector<PackedVertex>& out);

	public:

		ChunkRenderer(std::istream& in);
		~ChunkRenderer();

		bool needsUpdate = false;
		bool neighbourNeedUpdate[4] { false };
		bool blockLightOnlyRebuild = false;

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

		inline const GLuint getVao() const {return VAO;}
		inline const uint getMeshVertexCount() const {return meshVertexCount;}

		inline const GLuint getWaterVao() const {return waterVAO;}
		inline const uint getWaterMeshVertexCount() const {return waterMeshVertexCount;}

		inline const GLuint getPlacedWaterVao() const {return placedWaterVAO;}
		inline const uint getPlacedWaterMeshVertexCount() const {return placedWaterMeshVertexCount;}

		static int sSeaLevel;
		static void setSeaLevel(int sl) { sSeaLevel = sl; }

		inline VegetationRenderer* getVegetationRenderer() const { return vegetationRenderer.get(); }

		const std::vector<TorchInstance>& getTorchInstances() const { return torchInstances; }

		// Leaf-rendering strategy. Drives both mesh emission and fragment-shader
		// alpha test. Switching modes only affects the *next* mesh rebuild; for
		// instant effect call buildMesh() on every loaded chunk (or press F3+A).
		//
		//   Fast   - leaves treated as opaque blocks. Mesher culls leaf-to-leaf
		//            and solid-to-leaf faces. Fragment shader skips alpha test.
		//            Cheapest; leaves look like solid green cubes.
		//   Fancy  - leaves treated as transparent (original behaviour). Every
		//            face emitted, including leaves seen through other leaves.
		//            Most expensive; visually richest.
		//   Smart  - leaves alpha-tested (cutouts visible on outer faces) but
		//            mesher culls like Fast. Outer surface looks like leaves;
		//            inside each canopy is hollow.
		enum class LeafRenderMode { Fast, Fancy, Smart };
		static LeafRenderMode sLeafRenderMode;
};

#endif
