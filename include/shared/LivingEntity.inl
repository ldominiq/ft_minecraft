
#include "LivingEntity.hpp"

template <typename WorldT>
LivingEntity<WorldT>::LivingEntity(glm::vec3 position) : Entity<WorldT>(position)
{}

template <typename WorldT>
LivingEntity<WorldT>::~LivingEntity()
{}

