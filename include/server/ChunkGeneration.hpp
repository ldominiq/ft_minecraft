
#ifndef CHUNK_GENERATION_HPP
#define CHUNK_GENERATION_HPP

#include "TerrainParams.hpp"
#include "Noise.hpp"
#include "Chunk.hpp"

#include <algorithm>

class ChunkGeneration : public Chunk {

	TerrainGenerationParams currentParams;
	
	public:

		ChunkGeneration(const int chunkX, const int chunkZ, const TerrainGenerationParams& params, const bool doGenerate = true);

		void generate(const TerrainGenerationParams& terrainParams);
		void generateCaves(BlockStorage &blocks, const TerrainGenerationParams &terrainParams);
		void generateOres(BlockStorage &blocks, const TerrainGenerationParams &terrainParams);

		bool preGenerated = false;

		static float interpolateSpline(float noise, const std::vector<std::pair<float, float>>& spline);

		static float getContinentalness(const TerrainGenerationParams& terrainParams, float wx, float wz);
		static float getErosion(const TerrainGenerationParams& terrainParams, float wx, float wz);
		static float getPV(const TerrainGenerationParams& terrainParams, float wx, float wz);

		static float getTemperature(const TerrainGenerationParams& terrainParams, float wx, float wz);
		static float getHumidity(const TerrainGenerationParams& terrainParams, float wx, float wz);

		static float surfaceNoiseTransformation(float noise, int splineIndex);

		static int computeTerrainHeight(const TerrainGenerationParams& terrainParams, float worldX, float worldZ);
		static BiomeType computeBiome(const TerrainGenerationParams& terrainParams, float worldX, float worldZ, int height);

};

class BlockStorage {
	public:
		BlockStorage() : data(Chunk::WIDTH * Chunk::HEIGHT * Chunk::DEPTH, BlockType::AIR) {}

		BlockType& at(int x, int y, int z) {
			return data[x + Chunk::WIDTH * (y + Chunk::HEIGHT * z)];
		}
		const BlockType& at(int x, int y, int z) const {
			return data[x + Chunk::WIDTH * (y + Chunk::HEIGHT * z)];
		}

		const std::vector<BlockType> &getData() const {
			return data;
		}

	private:
		std::vector<BlockType> data;
};

#endif
