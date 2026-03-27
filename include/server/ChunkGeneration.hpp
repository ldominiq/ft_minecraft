#ifndef CHUNK_GENERATION_HPP
#define CHUNK_GENERATION_HPP

#include "../shared/TerrainParams.hpp"
#include "TerrainParams.hpp"  // Includes oreTable and Item.hpp for server builds
#include "Noise.hpp"
#include "Chunk.hpp"

#include <ranges>
#include <algorithm>

class ChunkGeneration : public Chunk {

	TerrainGenerationParams currentParams;
	
	public:

		ChunkGeneration(const int chunkX, const int chunkZ, const TerrainGenerationParams& params, const bool doGenerate = true);

		void generate(const TerrainGenerationParams& terrainParams);
		void generateTrees(BlockStorage &blocks, const TerrainGenerationParams &terrainParams) const;
		void placeTree(BlockStorage &blocks, int trunkWorldX, int trunkWorldZ, int surfaceY, int treeHeight, BlockType logType, BlockType leafType, int canopyStyle, int maxTrunkWidth, std::mt19937 &rng) const;
		void generateCaves(BlockStorage &blocks, const TerrainGenerationParams &terrainParams) const;
		void generateOres(BlockStorage &blocks, const TerrainGenerationParams &terrainParams) const;
		void generateVegetation(const BlockStorage &blocks, const TerrainGenerationParams &terrainParams);
		void generateCacti(BlockStorage &blocks, const TerrainGenerationParams &terrainParams) const;

		static float interpolateSpline(float noise, const std::vector<std::pair<float, float>>& spline);

		static float getContinentalness(const TerrainGenerationParams& terrainParams, float wx, float wz);
		static float getErosion(const TerrainGenerationParams& terrainParams, float wx, float wz);
		static float getPV(const TerrainGenerationParams& terrainParams, float wx, float wz);

		static float getTemperature(const TerrainGenerationParams& terrainParams, float wx, float wz);
		static float getHumidity(const TerrainGenerationParams& terrainParams, float wx, float wz);

		// Raw hydrology noise fields (for debug/image dumps)
		static float getRiverNoise(const TerrainGenerationParams& terrainParams, float wx, float wz);
		static float getLakeNoise(const TerrainGenerationParams& terrainParams, float wx, float wz);

		// Final masks used by terrain carving (0..1)
		static float getRiverMask(const TerrainGenerationParams& terrainParams, float worldX, float worldZ, float continentalness, float baseHeight, float pv);
		static float getLakeMask(const TerrainGenerationParams& terrainParams, float worldX, float worldZ, float continentalness, float baseHeight, float pv);

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
