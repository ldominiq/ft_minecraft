#ifndef ENTITY_HPP
#define ENTITY_HPP

#include <glm/glm.hpp>
#include <cmath>

#include "Network.hpp" // For inputs. Maybe should do it in some other way
#include "Block.hpp"

constexpr float EPS = 1e-5f;

//EFFECT MULTIPLIER
// TODO

//Slipperiness Multiplier
#define SM_DEFAULT		0.6f
#define SM_SLIME		0.8f
#define SM_ICE			0.98f
#define SM_AIRBORNE		1.0f

#define GRAVITY			0.08f
#define DRAG			0.98f

class World;

struct AABB {
    glm::vec3 min;
    glm::vec3 max;
    AABB() = default;
    AABB(const glm::vec3 &min_, const glm::vec3 &max_) : min(min_), max(max_) {}

    AABB movedBy(float dx, float dy, float dz) const {
        return AABB(min + glm::vec3(dx, dy, dz), max + glm::vec3(dx, dy, dz));
    }

    // strict overlap test (no touching)
    bool intersects(const AABB &o) const {
        return (min.x < o.max.x && max.x > o.min.x) &&
               (min.y < o.max.y && max.y > o.min.y) &&
               (min.z < o.max.z && max.z > o.min.z);
    }
};

class Entity {

	protected:
		float entityWidth;
		float entityHeight;

		float verticalVelocity;

		glm::vec3 position;

		bool onGround = true;

		inline bool isSolidBlock(BlockType b) { return b != BlockType::AIR; }

		AABB constructAABB(const glm::vec3 &pos);
		bool aabbCollidesWithWorld(const AABB &box, const std::unique_ptr<World> &world);

		virtual void calculateNewPosition(const std::unique_ptr<World> &world) = 0;
		void calculateNewXZPosition(const std::unique_ptr<World> &world, glm::vec3 &desiredMove);
		void calculateNewYPosition(const std::unique_ptr<World> &world);

	public:

		Entity(glm::vec3 position);
		virtual ~Entity() = 0;
};

#endif