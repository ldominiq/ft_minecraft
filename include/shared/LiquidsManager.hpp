
#ifndef LIQUIDS_MANAGER_HPP
#define LIQUIDS_MANAGER_HPP

#include <glm/glm.hpp>
#include <unordered_map>
#include <functional>
#include <memory>

#include "Item.hpp"

namespace std {
    template<>
    struct hash<glm::ivec3> {
        size_t operator()(const glm::ivec3& v) const noexcept {
            // Convert floats to integers in a stable way (bitwise hash)
            size_t h1 = std::hash<int>{}(v.x);
            size_t h3 = std::hash<int>{}(v.z);
            size_t h2 = std::hash<int>{}(v.y);
            
            // Combine hashes (a common method)
            size_t seed = h1;
            seed ^= h2 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            seed ^= h3 + 0x9e3779b9 + (seed << 6) + (seed >> 2);
            return seed;
        }
    };
}

struct s_waterPath
{
	glm::ivec3 currBlockPos{};
	std::vector<glm::ivec3> currPath{};
};

struct s_liquid {
	int currPropagation = ItemRegistry::getLiquid(BlockType::WATER).maxPropagation;
	std::weak_ptr<s_liquid> source;
	BlockType liquidType = BlockType::WATER;
	glm::ivec3 position{};
	std::vector<s_waterPath> currentPaths;
};

struct s_liquidsManager {
	std::unordered_map<glm::ivec3, std::shared_ptr<s_liquid>> liquidsToUpdate;
	std::unordered_map<glm::ivec3, std::shared_ptr<s_liquid>> liquids;
	int tickSinceLastUpdate = 0;

	void addNewLiquid(glm::ivec3 position, std::shared_ptr<s_liquid> liquidPtr)
	{
		if (liquidPtr->currPropagation >= 0)
		{
			liquids[position] = liquidPtr;
			liquidsToUpdate[position] = liquidPtr;
		}
	}
};

#endif