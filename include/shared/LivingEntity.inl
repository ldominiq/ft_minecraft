
#include "LivingEntity.hpp"

template <typename ChunkT>
LivingEntity<ChunkT>::LivingEntity(glm::vec3 position) : Entity<ChunkT>(position)
{}

template <typename ChunkT>
LivingEntity<ChunkT>::~LivingEntity()
{}

