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
    uint32_t acquire() {
        if (!freeIDs.empty()) {
            uint32_t id = freeIDs.front();
            freeIDs.pop();
            return id;
        }
        return nextID++;
    }

    void release(uint32_t id) {
        freeIDs.push(id);
    }

	private:
		uint32_t nextID = 1;                 // start from 1 (0 = invalid?)
		std::queue<uint32_t> freeIDs;        // recycled IDs
};

class Entity {

	static ItemEntityIDManager idManager;
	uint32_t ID;

	protected:
		float entityWidth;
		float entityHeight;

		glm::vec3 velocity = glm::vec3(0, 0, 0);
		float verticalVelocity = 0;

		glm::vec3 position;

		bool onGround = true;

		inline bool isSolidBlock(const BlockType &b) { return b != BlockType::AIR; }

		AABB constructAABB(const glm::vec3 &pos);
		bool aabbCollidesWithWorld(const AABB &box, const ICommonWorld &world);

		virtual glm::vec3 getDesiredMove() = 0;
		void calculateNewXZPosition(const ICommonWorld &world, glm::vec3 &desiredMove);
		void calculateNewYPosition(const ICommonWorld &world);

	public:
		Entity(glm::vec3 position);
		Entity(glm::vec3 position, uint32_t ID);
		virtual ~Entity() = 0;

		float yaw, pitch;
		bool entityCollidesWithBlock(const glm::vec3 blockPos);

		inline virtual EEntityTypes getEntityType() const = 0;
		inline virtual ItemID getItemType() const { return 0; } //only used for items; It's here to avoid the cost of dynamically down casting
		virtual void calculateNewPosition(const ICommonWorld &world);
		inline const glm::vec3 getPosition() const { return position; }
		inline const float getEntityWitdth() const { return entityWidth; }
		inline const float getEntityHeight() const { return entityHeight; }
		inline const uint32_t getID() const { return ID; }

		inline void setPosition(glm::vec3 position) {this->position = position; }

		virtual void draw(const glm::mat4 &projection, const glm::mat4 &view) { std::cout << "Code is crap :D" << std::endl; }; //ONLY USED IN CLIENT;
};

#endif