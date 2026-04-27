#ifndef ENTITY_HPP
#define ENTITY_HPP

#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>
#include <queue>
#include <iostream>
#include <deque>
#include <algorithm>

#include "Network.hpp" // For inputs. Maybe should do it in some other way
#include "Item.hpp"
#include "Chunk.hpp"
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

// AABB stored in double so that block-collision math (floor(min.x + EPS) ...)
// stays correct at very large world coordinates. With float, at 5M coords the
// LSB is ~0.5m and adding entityWidth*0.5 to a player's position rounds to a
// different block boundary on +X vs -X — manifesting as the player visually
// clipping into blocks asymmetrically.
struct AABB {
    glm::dvec3 min;
    glm::dvec3 max;
    AABB() = default;
    AABB(const glm::dvec3 &min_, const glm::dvec3 &max_) : min(min_), max(max_) {}
    AABB(const glm::vec3 &min_, const glm::vec3 &max_) : min(glm::dvec3(min_)), max(glm::dvec3(max_)) {}

    AABB movedBy(double dx, double dy, double dz) const {
        return AABB(min + glm::dvec3(dx, dy, dz), max + glm::dvec3(dx, dy, dz));
    }

    // strict overlap test (no touching)
    bool intersects(const AABB &o) const {
        return (min.x < o.max.x && max.x > o.min.x) &&
               (min.y < o.max.y && max.y > o.min.y) &&
               (min.z < o.max.z && max.z > o.min.z);
    }
};

class ICommonWorld;

//idManager is shared across all Entity instances. It does not support multithreading in this current form. It could lead to race conditions. Must be single threaded
class ItemEntityIDManager {

	entityID nextID = 1;                 // start from 1 (0 = invalid?)
	std::queue<entityID> freeIDs;        // recycled IDs

	public:
		entityID acquire() {
			if (!freeIDs.empty() && freeIDs.size() > 1000) { // 1000 offset so there's no risk of conflicts between clientIDs and server IDs reuses.
				entityID id = freeIDs.front();
				freeIDs.pop();
				return id;
			}
			return nextID++;
		}

		void release(entityID id) {
			freeIDs.push(id);
		}
};

class TextureManager;

//only used in client.
struct Snapshot
{
 glm::dvec3 position;
	glm::vec3 velocity;
	double time;
};

class Entity {

	static ItemEntityIDManager idManager;

	protected:
		entityID ID;

		float entityWidth = 0;
		float entityHeight = 0;

		glm::vec3 velocity{};

		// Stored in double precision so that movement at very large world
		// coordinates (millions of blocks from origin) does not snap to the
		// float-quantization grid (~0.06 units at x=1e6). Most callers still
		// see this as a glm::vec3 via the legacy getPosition() accessor.
		glm::dvec3 position{};

		float slipperiness_prev = SM_AIRBORNE;
		bool onGround = false;

		bool aabbCollidesWithWorld(const AABB &box, const ICommonWorld &world);

		virtual glm::vec3 getDesiredMove() = 0;
		void calculateNewXZPosition(const ICommonWorld &world, glm::vec3 &desiredMove);
		virtual void calculateNewYPosition(const ICommonWorld &world);
		virtual void calculateUnderwaterPosition(const ICommonWorld &world);

	public:
		Entity(const glm::vec3 &position);
		Entity(const glm::vec3 &position, float yaw, entityID ID);
		virtual ~Entity() = 0;

		float yaw = 0;
		float pitch = 0;

		AABB constructAABB(const glm::dvec3 &pos);
		AABB constructAABB(const glm::vec3 &pos) { return constructAABB(glm::dvec3(pos)); }
		bool entityCollidesWithBlock(const glm::vec3 blockPos);

		// position has been changed since last check.
		bool positionUpdated = true;
		// true when movement keys are actively pressed (set by PlayerMovement; defaults true for remote entities)
		bool hasHorizontalInput = true;
		// yaw/rotation has changed since last check (without a position change).
		bool rotationUpdated = false;
		// one-shot flag: player clicked to break/place/attack. Broadcast once then reset.
		bool pendingArmSwing = false;
		// timestamp of the last network position update (glfwGetTime / serverTime scale)
		double lastNetUpdateTime = -1.0;

		// unused. Supposed to be for prediction
		inline float getSlipperinessPrev() const { return slipperiness_prev; }
		inline bool isOnGround() const { return onGround; }

		void applyImpulse(const glm::vec3& impulse) { velocity += impulse; }
		inline virtual EEntityTypes getEntityType() const = 0;
		virtual void calculateNewPosition(const ICommonWorld &world);
		// Legacy getter — returns float-precision snapshot of the position.
		// Use getPositionD() when you need the precision (camera path, etc).
		inline const glm::vec3 getPosition() const { return glm::vec3(position); }
		inline const glm::dvec3 getPositionD() const { return position; }
		inline const float getEntityWidth() const { return entityWidth; }
		inline const float getEntityHeight() const { return entityHeight; }
		inline const entityID getID() const { return ID; }

		inline void setSlipperinessPrev(float slipperiness) { this->slipperiness_prev = slipperiness; }
		inline void setOnGround(bool value) { this->onGround = value; }
		float getDepthUnderwater() const;

		inline void setPosition(glm::vec3 position) {
			setPosition(glm::dvec3(position));
		}
		inline void setPosition(glm::dvec3 position) {
			if (this->position != position) positionUpdated = true;
			this->position = position;
		}

		inline ChunkPos getChunkPos() const {
			int chunkX = static_cast<int>(std::floor(position.x / static_cast<double>(Chunk::WIDTH)));
			int chunkZ = static_cast<int>(std::floor(position.z / static_cast<double>(Chunk::DEPTH)));
			return ChunkPos(chunkX, chunkZ);
		}

		//ONLY USED IN CLIENT :
		//TODO move all of this.
		std::deque<Snapshot> snapshots;
		
		bool removed = false; //item entities only
		
		virtual void lerp(double glfwTime)
		{
			if (snapshots.size() < 2) return;
			while (snapshots.size() > 2 && snapshots[1].time <= glfwTime) {
				snapshots.pop_front();
			}

			// std::cout << "Lerping entity " << ID << " with " << snapshots.size() << " snapshots\n";

			auto& start = snapshots[0];
			auto& end   = snapshots[1];

			double duration = end.time - start.time;
			if (duration <= 0.0) {
				setPosition(end.position);
				snapshots.pop_front();
				return;
			}
			double t = (glfwTime - start.time) / duration;
			t = std::clamp(t, 0.0, 1.0);

         glm::dvec3 interpolatedPosition = glm::mix(start.position, end.position, t);
			setPosition(interpolatedPosition);
		};
		//virtual void predict();

		// Item entities only. eyePos is the camera-relative origin: implementations
		// must emit vertices in (worldPos - eyePos) so positions stay precise at
		// large world coordinates. The mesh is rebuilt every frame anyway, so
		// passing eyePos here costs nothing.
		virtual void createMesh(std::vector<float> &meshVertices, const glm::dvec3& eyePos, const TextureManager* texMgr = nullptr) { (void)meshVertices; (void)eyePos; (void)texMgr; std::cout << "Not Yet Implemented :D" << std::endl; }; //item entities only
		virtual void draw(std::vector<float> &meshVertices) { std::cout << "Not Yet Implemented :D" << std::endl; }; //living entities only

		//the not yet Implemented is a lie. Those are only client functions defined in the client.

		bool doDraw = true; //this should kinda be private
		inline void setDoDraw(bool value) {doDraw = value;}
		inline bool DoDraw() const {return doDraw;}
};

#endif
