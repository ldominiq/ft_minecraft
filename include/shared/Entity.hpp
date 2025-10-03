#ifndef ENTITY_HPP
#define ENTITY_HPP

#include <glm/glm.hpp>
#include <cmath>
#include <queue>
#include <iostream>

#include "Network.hpp" // For inputs. Maybe should do it in some other way
#include "Item.hpp"
// #include "CommonWorld.hpp"

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

using entityID = uint32_t;

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

class ICommonWorld;

class ItemEntityIDManager {
public:
    entityID acquire() {
        if (!freeIDs.empty()) {
            entityID id = freeIDs.front();
            freeIDs.pop();
            return id;
        }
        return nextID++;
    }

    void release(entityID id) {
        freeIDs.push(id);
    }

	private:
		entityID nextID = 1;                 // start from 1 (0 = invalid?)
		std::queue<entityID> freeIDs;        // recycled IDs
};

class Entity {

	static ItemEntityIDManager idManager;

	protected:
		entityID ID;

		float entityWidth;
		float entityHeight;

		//TODO : maybe use velocity.y instead of verticalVelocity... velocity.y is always = 0
		glm::vec3 velocity = glm::vec3(0, 0, 0);
		float verticalVelocity = 0;

		glm::vec3 position;

		bool onGround = false;

		inline bool isSolidBlock(const BlockType &b) { return b != BlockType::AIR; }

		AABB constructAABB(const glm::vec3 &pos);
		bool aabbCollidesWithWorld(const AABB &box, const ICommonWorld &world);

		virtual glm::vec3 getDesiredMove() = 0;
		void calculateNewXZPosition(const ICommonWorld &world, glm::vec3 &desiredMove);
		void calculateNewYPosition(const ICommonWorld &world);

	public:
		Entity(glm::vec3 position);
		Entity(glm::vec3 position, entityID entityID);
		virtual ~Entity() = 0;

		float yaw, pitch;
		bool entityCollidesWithBlock(const glm::vec3 blockPos);

		inline virtual EEntityTypes getEntityType() const = 0;
		inline virtual BlockType getItemType() const { return BlockType::END; } //only used for items; It's here to avoid the cost of dynamically down casting
		virtual void calculateNewPosition(const ICommonWorld &world);
		inline const glm::vec3 getPosition() const { return position; }
		inline const float getEntityWidth() const { return entityWidth; }
		inline const float getEntityHeight() const { return entityHeight; }
		inline const entityID getID() const { return ID; }

		inline void setPosition(glm::vec3 position) {this->position = position; }

		//ONLY USED IN CLIENT :
		glm::vec3 prevPosition = glm::vec3(0.0f, 0.0f, 0.0f);
		virtual void draw(const glm::mat4 &projection, const glm::mat4 &view, const glm::vec3 &position) { ;} //std::cout << "Not Yet Implemented :D" << std::endl; };
};

#endif
