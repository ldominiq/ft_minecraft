#include "Renderer.hpp"
#include <algorithm>
#include <cmath>
#include <imgui.h>

void Renderer::setMSAAEnabled(bool enabled) {
	if (enabled) {
		glEnable(GL_MULTISAMPLE);
	} else {
		glDisable(GL_MULTISAMPLE);
	}
	m_msaaEnabled = enabled;
}

void Renderer::linkNeighbors(int chunkX, int chunkZ, std::shared_ptr<ChunkRenderer> &chunk) {

    const int dirX[] = { 0, 0, 1, -1 };
    const int dirZ[] = { 1, -1, 0, 0 };
    const int opp[]  = { SOUTH, NORTH, WEST, EAST };

    for (int dir = 0; dir < 4; ++dir) {
        int nx = chunkX + dirX[dir];
        int nz = chunkZ + dirZ[dir];

        std::shared_ptr<ChunkRenderer> neighbor = getChunk(nx, nz);

        chunk->setAdjacentChunks(static_cast<Direction>(dir), neighbor);
		if (chunk->hasAllAdjacentChunkLoaded())
			chunksToBuild.insert(Chunk::toKey(chunkX, chunkZ));
        if (neighbor) {
            neighbor->setAdjacentChunks(opp[dir], chunk);

            if (neighbor->hasAllAdjacentChunkLoaded()) {
				chunksToBuild.insert(Chunk::toKey(nx, nz));
            }
        }
    }
}

bool Renderer::setBlockWorld(glm::ivec3 globalCoords, std::optional<glm::ivec3> faceNormal, BlockType type)
{
    int x, y, z;
    auto currChunk = resolveTarget(globalCoords, faceNormal, x, y, z);
    if (!currChunk)
        return false;

    // Place/break the block, cascading to clear any land vegetation above when breaking.
    // Land vegetation is tracked only in the block grid; buildVegetationMesh() derives
    // instances by scanning blocks, so no separate vegetation list sync is needed.
    currChunk->setBlockCascade(x, y, z, type);
	currChunk->needsUpdate = true;

	// //update possible neighbour
	if (x == 0)
		currChunk->neighbourNeedUpdate[WEST] = true;
	if (x == Chunk::WIDTH - 1)
		currChunk->neighbourNeedUpdate[EAST] = true;
	if (z == 0)
		currChunk->neighbourNeedUpdate[SOUTH] = true;
	if (z == Chunk::DEPTH - 1)
		currChunk->neighbourNeedUpdate[NORTH] = true;

	return true;
}

std::vector<std::weak_ptr<ChunkRenderer>> Renderer::getRenderedChunks()
{
	return renderedChunks;
}

void Renderer::updateChunk(const NetModifiedBlockData &pkt)
{
	glm::vec3 targetCoords = glm::vec3(pkt.x, pkt.y, pkt.z);
	setBlockWorld(targetCoords, std::nullopt, static_cast<BlockType>(pkt.blockType));
}

void Renderer::buildChunks()
{
	// Collect all chunks that need building this frame.
	std::vector<std::pair<ChunkPos, std::shared_ptr<ChunkRenderer>>> toBuild;
	for (auto [chunkX, chunkZ] : chunksToBuild) {
		std::shared_ptr<ChunkRenderer> currChunk = getChunk(chunkX, chunkZ);
		if (!currChunk) continue;
		toBuild.push_back({{chunkX, chunkZ}, currChunk});
	}

	// Sky-light is already computed for each chunk in receiveChunk()
	// immediately after deserialization, so every chunk entering
	// buildChunks() via linkNeighbors() already has a valid skyLight
	// array.  No need to recompute here.

	// ── Build meshes (all skyLight arrays are valid) ────────────
	std::vector<std::future<ChunkPos>> meshFutures;
		for (auto& [pos, chunk] : toBuild) {
		auto cx = pos.first;
		auto cz = pos.second;
		auto chunkPtr = chunk; // structured bindings can't be captured directly
		meshFutures.push_back(std::async(std::launch::async, [chunkPtr, cx, cz]() {
			chunkPtr->buildMeshData();
			return Chunk::toKey(cx, cz);
		}));
				}

	for (auto it = meshFutures.begin(); it != meshFutures.end();) {
		ChunkPos pos = it->get();
		auto chunk = getChunk(pos.first, pos.second);
		if (chunk) {
			chunk->uploadMesh();
			chunk->buildVegetationMesh(); // Build vegetation after mesh is uploaded
			chunks[{pos.first, pos.second}] = chunk;
		}
		it = meshFutures.erase(it);
	}
	chunksToBuild.clear();
}

//sets rendered chunks and unloads far away chunks
void Renderer::organizeChunks(const std::pair<int, int> pos, int loadRadius, float deltaTime)
{
    // Clear renderedChunks first
    renderedChunks.clear();

	int unloadRadius = loadRadius * 4;
    const int radiusSq = loadRadius * loadRadius;

    // Don't evict chunks that arrived in the last few seconds: the player
    // position can lag a teleport/respawn by a handful of frames (NetPlayerMove
    // is unreliable), and during that window freshly-received chunks would be
    // erased by distance even though the server already marked them sent — so
    // they'd never be re-streamed and the area would have permanent holes.
    constexpr auto RECENT_CHUNK_GRACE = std::chrono::seconds(3);
    const auto now = std::chrono::steady_clock::now();

    for (auto it = chunks.begin(); it != chunks.end(); )
    {
        const ChunkPos& chunkPos = it->first;
        auto& chunkPtr = it->second;

        // Compute squared distance between chunk coordinates
        int dx = chunkPos.first - pos.first;
        int dz = chunkPos.second - pos.second;
        int distSq = dx * dx + dz * dz;

        if (distSq <= radiusSq)
        {
            // Inside load radius -> render
            renderedChunks.push_back(chunkPtr);
            ++it;
        }
        else if (distSq >= unloadRadius * unloadRadius)
        {
            auto rt = chunkReceiveTime.find(chunkPos);
            if (rt != chunkReceiveTime.end() && now - rt->second < RECENT_CHUNK_GRACE)
            {
                // Recently arrived — keep it; player position may still be
                // catching up after a teleport/respawn.
                ++it;
            }
            else
            {
                // Outside unload radius and old enough -> remove chunk
                chunkReceiveTime.erase(chunkPos);
                it = chunks.erase(it);
            }
        }
        else
        {
            // In between -> keep chunk loaded but not rendered
            ++it;
        }
    }

    // Find the nearest chunk position within the load radius that has NOT yet
    // been received from the server.  Fog is placed at that boundary so any
    // unloaded area is always hidden while the world generates.
    int minMissingDistSq = radiusSq; // default: assume fully loaded
    for (int dx = -loadRadius; dx <= loadRadius; ++dx) {
        for (int dz = -loadRadius; dz <= loadRadius; ++dz) {
            int dSq = dx * dx + dz * dz;
            if (dSq > radiusSq) continue;
            ChunkPos testPos = {pos.first + dx, pos.second + dz};
            if (chunks.find(testPos) == chunks.end()) {
                if (dSq < minMissingDistSq)
                    minMissingDistSq = dSq;
            }
        }
    }
    float rawDist = std::sqrt(static_cast<float>(minMissingDistSq)) * Chunk::WIDTH;
    
    // we could initialize immediately to avoid a long fade-in on startup but i think i like it
    // if (maxRenderedChunkDist <= 0.0f) {
        //     maxRenderedChunkDist = rawDist;
    // } else {

        // Use different speeds for inward vs outward movement but always smooth
        // Inward: ~0.33s time constant, fast enough to cover a gap before the player walks into it.
        // Outward: ~2s time constant, slow enough that loading chunks don't flicker.
        float speed = (rawDist < maxRenderedChunkDist) ? 3.0f : 0.5f;
        float alpha = 1.0f - std::exp(-deltaTime * speed);
        maxRenderedChunkDist += (rawDist - maxRenderedChunkDist) * alpha;

    // }
}

void Renderer::prepareChunk(const NetChunkHeader& pkt) {
	chunkData data;
	data.compressedSize = pkt.compressedSize;
	data.uncompressedSize = pkt.uncompressedSize;
	data.chunkBuffer.reserve(pkt.compressedSize);
	chunksData[std::make_pair(pkt.X, pkt.Z)] = data;
}

// TODO : Make this function great. could be fucked up packet > MAXLINE and 1 packet gets lost
void Renderer::receiveChunk(const NetChunkData& pkt) {
	auto data = chunksData.find(std::make_pair(pkt.X, pkt.Z));
	if (data == chunksData.end()) // Will never not find an element because of chunk 0, 0. And X and Z initialise to 0. TODO : fix this
		return ;

	data->second.chunkBuffer.insert(data->second.chunkBuffer.end(), pkt.data.begin(), pkt.data.end());

    // If last packet for this chunk (we might need a "final packet" flag later)
    if ((pkt.flags & PacketFlags::FinalChunk) != PacketFlags::None) { // define this flag
        std::vector<uint8_t>& compressed = data->second.chunkBuffer;
        // Decompress
        std::vector<uint8_t> decompressed(data->second.uncompressedSize);
        size_t res = ZSTD_decompress(decompressed.data(), decompressed.size(), compressed.data(), compressed.size()); // TODO : maybe use decompres with a dict. would be better
		if (ZSTD_isError(res)) {
			std::cerr << "Decompression failed: " 
					<< ZSTD_getErrorName(res) << "\n"
					<< "Expected size: " << data->second.uncompressedSize
					<< ", got: " << res << "\n"
					<< "Compressed size: " << compressed.size() << "\n";
			return;
		}

        // Deserialize into BitPackedArray
        std::istringstream iss(std::string(decompressed.begin(), decompressed.end()), std::ios::binary);

        std::shared_ptr<ChunkRenderer> newChunk = std::make_shared<ChunkRenderer>(iss);
		newChunk->setTextureManager(textureManager);
		// Compute sky-light immediately so that any neighbor chunk
		// building its mesh later can read valid skyLight values
		// from this chunk, even if this chunk isn't in chunksToBuild
		// yet (e.g. it doesn't have all 4 neighbors loaded).
		// Without this, neighbors would read fallback=15 from our
		// empty skyLight array and their border faces would look
		// incorrectly sunlit.
		newChunk->computeSkyLight();
		linkNeighbors(pkt.X, pkt.Z, newChunk);
		chunks[{pkt.X, pkt.Z}] = newChunk;
		chunkReceiveTime[{pkt.X, pkt.Z}] = std::chrono::steady_clock::now();
		// newChunk->buildMesh();
        data->second.chunkBuffer.clear();
    }
}

//I dislike having VAO here. TODO : MAYBE MAYBE change it
void Renderer::draw(const std::shared_ptr<Shader>& shader, const GLuint &VAO, const uint &vertexCount) const {
    shader->use();
    glBindVertexArray(VAO);
    glDrawArrays(GL_TRIANGLES, 0, vertexCount); // packed vertex format: count is exact vertex count
	m_drawCallCount++;
}

void Renderer::updateVegetationUniforms(const glm::mat4& view, const glm::mat4& projection,
                                        const glm::vec4& clipPlane, const glm::vec3& viewPos) const {
	if (!vegetationShader) return;
	vegetationShader->use();
    glm::mat4 viewRot = view;
    viewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	vegetationShader->setMat4("view", view);
    vegetationShader->setMat4("viewRot", viewRot);
	vegetationShader->setMat4("projection", projection);
	vegetationShader->setVec4("clipPlane", clipPlane);
	vegetationShader->setVec3("viewPos", viewPos);
	// Graphics-quality knobs — uploaded here too so vegetation in water
	// refraction respects the same sway/density settings as the main pass.
	vegetationShader->setInt  ("vegetationSwayQuality", vegetationSwayQuality);
	vegetationShader->setFloat("vegetationSwayMaxDist", vegetationSwayMaxDistance);
	vegetationShader->setInt  ("vegetationDensity",     vegetationDensity);
}

void Renderer::processMeshUpdates() {
	for (auto& weakChunk : renderedChunks) {
		if (auto chunk = weakChunk.lock())
			if (chunk->needsUpdate)
				chunk->updateMesh();
	}
}

void Renderer::renderTerrainOnly(const std::shared_ptr<Shader>& shaderProgram,
                                 const glm::mat4& view,
                                 const glm::dvec3& eyePos) const {
	// Build "viewRot": world view with translation column zeroed, i.e. the
	// camera placed at the origin of render space with the same orientation.
	glm::mat4 viewRot = view;
	viewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);

	// `eyePos` is supplied by the caller in double precision so that
	// (chunkOrigin - eye) keeps sub-cm precision even at very large world
	// coordinates. We must NOT recover it from `view` itself, because the
	// view matrix's translation column is float-quantized.
	const glm::dvec3 cameraPos = eyePos;

	shaderProgram->use();
	shaderProgram->setMat4("viewRot", viewRot);

	// Squared distance cap avoids sqrt in the per-chunk loop.
	const float chunkMaxDistSq = (maxRenderDistanceOverride > 0.0f)
		? maxRenderDistanceOverride * maxRenderDistanceOverride : 0.0f;

	m_lastVisibleChunks.clear();

	for (auto& weakChunk : renderedChunks) {
		auto chunk = weakChunk.lock();
		if (!chunk || chunk->getMeshVertexCount() == 0)
			continue;

		// Optional per-pass distance cap (e.g. water reflection wants only
		// nearby chunks rendered into its tiny offscreen target).
		const glm::dvec3 chunkOriginWorldD(static_cast<double>(chunk->getOriginX()), 0.0,
		                                   static_cast<double>(chunk->getOriginZ()));
		const glm::dvec3 chunkRelD = chunkOriginWorldD - cameraPos;
		const float distSq = static_cast<float>(chunkRelD.x * chunkRelD.x + chunkRelD.z * chunkRelD.z);
		if (chunkMaxDistSq > 0.0f && distSq > chunkMaxDistSq)
			continue;

		// Frustum cull: skip chunks entirely outside the camera view
		if (frustumCullingEnabled) {
			const glm::dvec3 minRelD = glm::dvec3(chunk->getCachedMinP()) - cameraPos;
			const glm::dvec3 maxRelD = glm::dvec3(chunk->getCachedMaxP()) - cameraPos;
			if (!cameraFrustum.isBoxVisible(glm::vec3(minRelD), glm::vec3(maxRelD)))
				continue;
		}

		// Per-chunk uniforms for camera-relative rendering.
		shaderProgram->setVec3("chunkRel", glm::vec3(chunkRelD));
		shaderProgram->setVec3("chunkOriginWorld", glm::vec3(chunkOriginWorldD));

		draw(shaderProgram, chunk->getVao(), chunk->getMeshVertexCount());
		m_lastVisibleChunks.push_back(chunk);
	}
}

void Renderer::renderVegetationOnly(const glm::mat4& view, const glm::dvec3& eyePos) const {
	if (!vegetationShader || m_lastVisibleChunks.empty())
		return;

	glm::mat4 viewRot = view;
	viewRot[3] = glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
	const glm::dvec3 cameraPos = eyePos;

	const float vegMaxDistSq = (vegetationMaxDistance > 0.0f)
		? vegetationMaxDistance * vegetationMaxDistance : 0.0f;

	vegetationShader->use();
	vegetationShader->setMat4("viewRot", viewRot);
	vegetationShader->setVec3("viewPos", glm::vec3(cameraPos));

	for (auto& chunk : m_lastVisibleChunks) {
		const glm::dvec3 chunkOriginWorldD(static_cast<double>(chunk->getOriginX()), 0.0,
		                                   static_cast<double>(chunk->getOriginZ()));
		const glm::dvec3 chunkRelD = chunkOriginWorldD - cameraPos;
		// Vegetation distance cap — separate from terrain so the user can
		// keep distant terrain visible while killing distant leaf overdraw.
		if (vegMaxDistSq > 0.0f) {
			const float dSq = static_cast<float>(chunkRelD.x * chunkRelD.x + chunkRelD.z * chunkRelD.z);
			if (dSq > vegMaxDistSq) continue;
		}
		vegetationShader->setVec3("chunkRel", glm::vec3(chunkRelD));
		vegetationShader->setVec3("chunkOriginWorld", glm::vec3(chunkOriginWorldD));
		auto vegRenderer = chunk->getVegetationRenderer();
		if (vegRenderer && vegRenderer->getInstanceCount() > 0)
			vegRenderer->render();
	}
}

void Renderer::render(const std::shared_ptr<Shader> &shaderProgram,
                      const glm::mat4& view,
                      const glm::dvec3& eyePos,
                      bool renderVegetation) const {
	// Convenience wrapper used by passes that don't want a Z-prepass
	// (water reflection/refraction, GBuffer, etc.).
	renderTerrainOnly(shaderProgram, view, eyePos);
	if (renderVegetation && vegetationShader) {
		renderVegetationOnly(view, eyePos);
		// Restore the caller's shader for any subsequent uniform binds.
		shaderProgram->use();
	}
}

void Renderer::renderShadow(const std::shared_ptr<Shader> &shaderProgram, const glm::mat4 &lightSpaceMatrix,
                            const glm::dvec3& eyePos) const {
	for (auto& weakChunk : renderedChunks) {
		auto chunk = weakChunk.lock();
		if (!chunk)
			continue;

		// Skip empty chunks (no geometry to cast shadows)
		if (chunk->getMeshVertexCount() == 0)
			continue;

		// Frustum cull: test the chunk AABB against the light's clip volume.
		// Chunk world-space AABB:
		const float x0 = chunk->getCachedMinP().x;
		const float z0 = chunk->getCachedMinP().z;
		const float x1 = chunk->getCachedMaxP().x;
		const float z1 = chunk->getCachedMaxP().z;
		constexpr float y0 = 0.0f;
		constexpr float y1 = static_cast<float>(Chunk::HEIGHT);

		// Transform all 8 AABB corners into light clip space and compute
		// the min/max of the resulting NDC coordinates.
		float clipMinX =  1e30f, clipMaxX = -1e30f;
		float clipMinY =  1e30f, clipMaxY = -1e30f;
		float clipMinZ =  1e30f, clipMaxZ = -1e30f;

		const glm::vec3 corners[8] = {
			{x0, y0, z0}, {x1, y0, z0}, {x0, y1, z0}, {x1, y1, z0},
			{x0, y0, z1}, {x1, y0, z1}, {x0, y1, z1}, {x1, y1, z1},
		};

		for (const auto& c : corners) {
         glm::dvec3 cRelD = glm::dvec3(c) - eyePos;
            glm::vec4 clip = lightSpaceMatrix * glm::vec4(glm::vec3(cRelD), 1.0f);
			// Ortho projection has w=1, but be safe
			float invW = 1.0f / clip.w;
			float nx = clip.x * invW;
			float ny = clip.y * invW;
			float nz = clip.z * invW;
			clipMinX = std::min(clipMinX, nx); clipMaxX = std::max(clipMaxX, nx);
			clipMinY = std::min(clipMinY, ny); clipMaxY = std::max(clipMaxY, ny);
			clipMinZ = std::min(clipMinZ, nz); clipMaxZ = std::max(clipMaxZ, nz);
		}

		// If the AABB is entirely outside any clip plane, skip this chunk.
		if (clipMaxX < -1.0f || clipMinX > 1.0f ||
			clipMaxY < -1.0f || clipMinY > 1.0f ||
			clipMaxZ < -1.0f || clipMinZ > 1.0f)
			continue;

		// Mesh is in chunk-local space — supply the world origin so the
		// vertex shader can reconstruct world positions before projecting
		// into the light's clip space.
      const glm::dvec3 chunkOriginWorldD(static_cast<double>(chunk->getOriginX()), 0.0,
                                           static_cast<double>(chunk->getOriginZ()));
        shaderProgram->setVec3("chunkRel", glm::vec3(chunkOriginWorldD - eyePos));
		draw(shaderProgram, chunk->getVao(), chunk->getMeshVertexCount());
	}
}

void Renderer::onEntity(NetEntityMove &pkt, double serverTime)
{
    glm::dvec3 position(pkt.positionX, pkt.positionY, pkt.positionZ);
	entityID ID = pkt.entityID;
	float yaw = pkt.yaw;

	auto entity = entitiesMap.find(ID);
    std::shared_ptr<Entity> ent;

    // Resolve existing entity (or clear stale weak entry)
    if (entity != entitiesMap.end())
    {
        ent = entity->second.lock();
        if (!ent)
            entitiesMap.erase(entity);
    }

    // Update existing entity
	if (ent) {
		const double oneTick = 1.0 / TPS;
		bool stale = ent->snapshots.empty()
			        || ent->lastNetUpdateTime < 0.0
			        || ent->snapshots.back().time < serverTime - 2.0 * oneTick;
		if (stale) {
			ent->snapshots.clear();
           ent->snapshots.emplace_back(Snapshot{ent->getPositionD(), glm::vec3(0.0f), serverTime - oneTick});
		}
		bool actuallyMoved = !ent->snapshots.empty() &&
            glm::length(position - ent->snapshots.back().position) > 0.001;
		ent->snapshots.emplace_back(Snapshot{position, glm::vec3(0.0f), serverTime});
		ent->yaw = yaw;
		ent->pitch = pkt.pitch;
		if (actuallyMoved)
			ent->positionUpdated = true;
		else
			ent->rotationUpdated = true;
		ent->hasHorizontalInput = (pkt.positionFlags & 0x01) != 0;
		ent->setOnGround((pkt.positionFlags & 0x02) != 0);
		// IClientEntity virtually inherits LivingEntity (the diamond with Creeper
		// forces it), so static_pointer_cast can't cross the virtual base — the
		// downcast needs RTTI. Done once here and reused for arm swing / death /
		// creeper below. Hoisting triggerArmSwing+triggerDeath onto Entity would
		// dodge the cast, but they're animation hooks meaningless for items and
		// would further pollute the shared header (see TODO in Entity.hpp).
		std::shared_ptr<IClientEntity> ice;
		if (pkt.eEntityType == EEntityTypes::LIVING_ENTITIES)
			ice = std::dynamic_pointer_cast<IClientEntity>(ent);

		if (ice) {
			if ((pkt.positionFlags & 0x04) != 0)
				ice->triggerArmSwing();
			if (pkt.type == static_cast<uint16_t>(CREEPER))
				std::static_pointer_cast<ClientCreeper>(ice)->clientPrimed = (pkt.positionFlags & 0x08) != 0;
		}
		ent->lastNetUpdateTime = serverTime;

		if (pkt.type == static_cast<uint16_t>(-1))
		{
			if (ice)
			{
				// Start the fall-over death animation instead of erasing immediately.
				// The LivingEntitiesManager flips `removed` once dyingDone, and
				// Renderer::drawCharacters sweeps removed entries afterwards.
				ice->triggerDeath();
			}
			else
			{
				ent->removed = true;
			}
		}
        return;
	}

	if (pkt.type == static_cast<uint16_t>(-1))  
        return; // New entity with type -1 means it's already dead, so ignore.

    // Create missing entity immediately (fixes player appearing only after moving)
	if (pkt.eEntityType == EEntityTypes::ITEMS)
	{
		ItemType type = itemIDToItemType(pkt.type);
     auto entityPtr = std::make_shared<ItemPropEntity>(glm::vec3(position), yaw, type, ID);
		entityPtr->setPosition(position);
		itemEntities.push_back(entityPtr);
		entitiesMap[ID] = entityPtr;
	}
	else if (pkt.eEntityType == EEntityTypes::LIVING_ENTITIES)
	{
		LivingEntityType type = static_cast<LivingEntityType>(pkt.type);
		std::shared_ptr<IClientEntity> entityPtr;
		switch (type)
		{
			case PLAYER:
              entityPtr = std::make_shared<ClientPlayer>(glm::vec3(position), yaw, ID);
				break;
			case CREEPER:
             entityPtr = std::make_shared<ClientCreeper>(glm::vec3(position), yaw, ID);
				break;
			case ZOMBIE:
              entityPtr = std::make_shared<ClientZombie>(glm::vec3(position), yaw, ID);
				break;
			default:
				std::cout << "ERROR ERROR MAYDAY WE GOT A PROBLEM" << std::endl;
				return;
		}

		entityPtr->setPosition(position);
		entityPtr->positionUpdated = true;
		entityPtr->snapshots.emplace_back(Snapshot{position, glm::vec3(0.0f), serverTime});
		livingEntitiesManager.add(entityPtr);

		// Convert to shared_ptr<LivingEntity> safely
		std::shared_ptr<LivingEntity> le = static_cast<std::shared_ptr<LivingEntity>>(entityPtr);
		livingEntities.push_back(le);
		entitiesMap[ID] = le;
	}
}

void Renderer::drawCharacters(const glm::mat4 &projection, const glm::mat4 &view,
                              const glm::dvec3& eyePos, const float deltatime)
{
    livingEntitiesManager.draw(projection, view, eyePos, deltatime);
	// Drop entities whose death animation completed (LivingEntitiesManager marks them).
	std::erase_if(livingEntities,
	              [](const std::shared_ptr<Entity>& e){ return !e || e->removed; });
}

// Frustum-cull a single water chunk's AABB against the cached frustum.
// Returns true if the chunk should be skipped this pass. Shared by all four
// water entry points (render + has-visible × ocean + placed) to keep the
// culling rule in one place.
bool Renderer::waterChunkCulled(const ChunkRenderer& chunk) const {
	if (!frustumCullingEnabled) return false;
	const glm::dvec3 minRelD = glm::dvec3(chunk.getCachedMinP()) - frustumEyePos;
	const glm::dvec3 maxRelD = glm::dvec3(chunk.getCachedMaxP()) - frustumEyePos;
	return !cameraFrustum.isBoxVisible(glm::vec3(minRelD), glm::vec3(maxRelD));
}

// One core loop for both ocean and placed-water draws. The bucket selector
// returns (VAO, vertexCount) for whichever mesh this pass owns; everything
// else (cull, chunk-relative uniforms, GL state) is identical.
template <typename BucketFn>
void Renderer::drawWaterBucket(Shader& shaderProgram, const glm::dvec3& eyePos,
                               BucketFn bucket) const {
	glDisable(GL_CULL_FACE);
	for (const auto& weakChunk : renderedChunks) {
		auto chunk = weakChunk.lock();
		if (!chunk) continue;
		const auto [vao, vertexCount] = bucket(*chunk);
		if (vertexCount == 0) continue;
		if (waterChunkCulled(*chunk)) continue;

		const glm::dvec3 chunkOriginWorldD(static_cast<double>(chunk->getOriginX()), 0.0,
		                                   static_cast<double>(chunk->getOriginZ()));
		shaderProgram.setVec3("chunkRel", glm::vec3(chunkOriginWorldD - eyePos));
		shaderProgram.setVec3("chunkOriginWorld", glm::vec3(chunkOriginWorldD));

		glBindVertexArray(vao);
		glDrawArrays(GL_TRIANGLES, 0, vertexCount);
	}
	glEnable(GL_CULL_FACE);
}

template <typename CountFn>
bool Renderer::anyVisibleWater(CountFn count) const {
	for (const auto& weakChunk : renderedChunks) {
		auto chunk = weakChunk.lock();
		if (!chunk) continue;
		if (count(*chunk) == 0) continue;
		if (waterChunkCulled(*chunk)) continue;
		return true;
	}
	return false;
}

void Renderer::renderWater(Shader& shaderProgram, const glm::dvec3& eyePos) const {
	drawWaterBucket(shaderProgram, eyePos, [](const ChunkRenderer& c) {
		return std::pair{c.getWaterVao(), c.getWaterMeshVertexCount()};
	});
}

bool Renderer::hasVisibleWater() const {
	return anyVisibleWater([](const ChunkRenderer& c) { return c.getWaterMeshVertexCount(); });
}

void Renderer::renderPlacedWater(Shader& shaderProgram, const glm::dvec3& eyePos) const {
	drawWaterBucket(shaderProgram, eyePos, [](const ChunkRenderer& c) {
		return std::pair{c.getPlacedWaterVao(), c.getPlacedWaterMeshVertexCount()};
	});
}

bool Renderer::hasVisiblePlacedWater() const {
	return anyVisibleWater([](const ChunkRenderer& c) { return c.getPlacedWaterMeshVertexCount(); });
}

// ---------------------------------------------------------------------------
// Frustum Culling Debug Radar – top-down ImGui view showing chunks coloured
// by visibility (green = rendered, red = culled).
// ---------------------------------------------------------------------------
void Renderer::drawFrustumCullingDebug(const glm::vec3& cameraPos,
                                       const glm::vec3& cameraFront,
                                       float fovDeg, float aspectRatio,
                                       float nearP, float farP)
{
    static float zoomLevel = 1.0f;
    static bool  showGrid  = true;

    ImGui::SetNextWindowSize(ImVec2(440, 540), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImVec2(450, 10), ImGuiCond_FirstUseEver);

    if (!ImGui::Begin("Frustum Culling Radar")) {
        ImGui::End();
        return;
    }

    // --- Controls ---
    ImGui::Checkbox("Enable Frustum Culling", &frustumCullingEnabled);
    if (!frustumCullingEnabled)
        ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "CULLING OFF – all chunks rendered (compare FPS)");
    ImGui::SameLine();
    ImGui::Checkbox("Grid", &showGrid);
    ImGui::SliderFloat("Zoom", &zoomLevel, 0.1f, 10.0f, "%.1fx");

    // --- Canvas setup ---
    ImVec2 canvasPos  = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    float  statsHeight = 80.0f;
    float  side = std::min(canvasSize.x, canvasSize.y - statsHeight);
    if (side < 80.0f) { ImGui::End(); return; }

    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 center(canvasPos.x + side * 0.5f, canvasPos.y + side * 0.5f);

    // World-space radius shown on the radar
    float worldRadius = (farP / zoomLevel) * 1.1f;

    drawList->PushClipRect(canvasPos, ImVec2(canvasPos.x + side, canvasPos.y + side), true);

    // Background
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + side, canvasPos.y + side),
                            IM_COL32(15, 15, 20, 240));

    // World XZ → screen
    auto worldToRadar = [&](float wx, float wz) -> ImVec2 {
        float dx = wx - cameraPos.x;
        float dz = wz - cameraPos.z;
        float sx = center.x + (dx / worldRadius) * (side * 0.45f);
        float sy = center.y + (dz / worldRadius) * (side * 0.45f);
        return ImVec2(sx, sy);
    };

    // --- Grid ---
    if (showGrid) {
        float gridSpacing = 16.0f; // one chunk
        if (worldRadius > 400)  gridSpacing = 64.0f;
        if (worldRadius > 800)  gridSpacing = 128.0f;

        float startW = floorf((cameraPos.x - worldRadius) / gridSpacing) * gridSpacing;
        float endW   = ceilf((cameraPos.x + worldRadius) / gridSpacing) * gridSpacing;
        for (float w = startW; w <= endW; w += gridSpacing) {
            ImVec2 a = worldToRadar(w, cameraPos.z - worldRadius);
            ImVec2 b = worldToRadar(w, cameraPos.z + worldRadius);
            drawList->AddLine(a, b, IM_COL32(40, 40, 40, 100), 1.0f);
        }
        startW = floorf((cameraPos.z - worldRadius) / gridSpacing) * gridSpacing;
        endW   = ceilf((cameraPos.z + worldRadius) / gridSpacing) * gridSpacing;
        for (float w = startW; w <= endW; w += gridSpacing) {
            ImVec2 a = worldToRadar(cameraPos.x - worldRadius, w);
            ImVec2 b = worldToRadar(cameraPos.x + worldRadius, w);
            drawList->AddLine(a, b, IM_COL32(40, 40, 40, 100), 1.0f);
        }
    }

    // --- Draw chunks ---
    int totalChunks   = 0;
    int visibleChunks = 0;
    int culledChunks  = 0;
    int emptyChunks   = 0;

    for (auto& weakChunk : renderedChunks) {
        auto chunk = weakChunk.lock();
        if (!chunk) continue;

        const float x0 = static_cast<float>(chunk->getOriginX());
        const float z0 = static_cast<float>(chunk->getOriginZ());
        const float x1 = x0 + static_cast<float>(Chunk::WIDTH);
        const float z1 = z0 + static_cast<float>(Chunk::DEPTH);

        const glm::vec3 minP(x0, 0.0f, z0);
        const glm::vec3 maxP(x1, Chunk::HEIGHT, z1);

        bool empty   = (chunk->getMeshVertexCount() == 0);
        bool visible = cameraFrustum.isBoxVisible(minP, maxP);

        totalChunks++;
        if (empty) {
            emptyChunks++;
        } else if (visible) {
            visibleChunks++;
        } else {
            culledChunks++;
        }

        ImVec2 tl = worldToRadar(x0, z0);
        ImVec2 br = worldToRadar(x1, z1);

        ImU32 fillColor;
        if (empty)
            fillColor = IM_COL32(60, 60, 60, 40);   // grey – empty
        else if (!frustumCullingEnabled)
            fillColor = IM_COL32(255, 200, 50, 50);  // yellow – all drawn (culling off)
        else if (visible)
            fillColor = IM_COL32(50, 200, 50, 60);   // green – rendered
        else
            fillColor = IM_COL32(200, 50, 50, 60);   // red – culled

        drawList->AddRectFilled(tl, br, fillColor);

        ImU32 outlineColor;
        if (empty)
            outlineColor = IM_COL32(80, 80, 80, 60);
        else if (!frustumCullingEnabled)
            outlineColor = IM_COL32(255, 200, 50, 120);
        else if (visible)
            outlineColor = IM_COL32(50, 200, 50, 140);
        else
            outlineColor = IM_COL32(200, 50, 50, 140);

        drawList->AddRect(tl, br, outlineColor, 0.0f, 0, 1.0f);
    }

    // --- Camera frustum outline (top-down trapezoid) ---
    {
        float halfFovRad = glm::radians(fovDeg * 0.5f);
        float halfHFov = atanf(tanf(halfFovRad) * aspectRatio);

        glm::vec2 fwd2D(cameraFront.x, cameraFront.z);
        float fwdLen = glm::length(fwd2D);
        if (fwdLen > 0.001f) {
            fwd2D /= fwdLen;
            glm::vec2 right2D(-fwd2D.y, fwd2D.x);

            float cosH = cosf(halfHFov);
            float sinH = sinf(halfHFov);

            glm::vec2 leftDir  = fwd2D * cosH - right2D * sinH;
            glm::vec2 rightDir = fwd2D * cosH + right2D * sinH;

            glm::vec2 nL = glm::vec2(cameraPos.x, cameraPos.z) + leftDir  * nearP;
            glm::vec2 nR = glm::vec2(cameraPos.x, cameraPos.z) + rightDir * nearP;
            glm::vec2 fL = glm::vec2(cameraPos.x, cameraPos.z) + leftDir  * farP;
            glm::vec2 fR = glm::vec2(cameraPos.x, cameraPos.z) + rightDir * farP;

            ImVec2 pts[4] = {
                worldToRadar(nL.x, nL.y),
                worldToRadar(fL.x, fL.y),
                worldToRadar(fR.x, fR.y),
                worldToRadar(nR.x, nR.y),
            };

            drawList->AddConvexPolyFilled(pts, 4, IM_COL32(255, 255, 255, 15));
            drawList->AddPolyline(pts, 4, IM_COL32(255, 255, 100, 200), ImDrawFlags_Closed, 1.5f);
        }
    }

    // --- Player dot ---
    drawList->AddCircleFilled(center, 5.0f, IM_COL32(255, 255, 255, 255));
    drawList->AddCircle(center, 5.0f, IM_COL32(0, 0, 0, 200), 0, 1.5f);

    // --- Camera direction arrow ---
    {
        glm::vec2 fwd(cameraFront.x, cameraFront.z);
        float fwdLen = glm::length(fwd);
        if (fwdLen > 0.001f) {
            fwd /= fwdLen;
            float arrowLen = side * 0.08f;
            ImVec2 tip(center.x + fwd.x * arrowLen, center.y + fwd.y * arrowLen);
            drawList->AddLine(center, tip, IM_COL32(255, 255, 255, 230), 2.5f);
            glm::vec2 perp(-fwd.y, fwd.x);
            float hs = 6.0f;
            ImVec2 left (tip.x - fwd.x * hs + perp.x * hs * 0.5f,
                         tip.y - fwd.y * hs + perp.y * hs * 0.5f);
            ImVec2 right(tip.x - fwd.x * hs - perp.x * hs * 0.5f,
                         tip.y - fwd.y * hs - perp.y * hs * 0.5f);
            drawList->AddTriangleFilled(tip, left, right, IM_COL32(255, 255, 255, 230));
        }
    }

    // --- Mouse hover: world coordinate ---
    ImVec2 mousePos = ImGui::GetMousePos();
    if (mousePos.x >= canvasPos.x && mousePos.x < canvasPos.x + side &&
        mousePos.y >= canvasPos.y && mousePos.y < canvasPos.y + side) {
        float relX = (mousePos.x - center.x) / (side * 0.45f) * worldRadius + cameraPos.x;
        float relZ = (mousePos.y - center.y) / (side * 0.45f) * worldRadius + cameraPos.z;
        char coordTxt[64];
        snprintf(coordTxt, sizeof(coordTxt), "(%.0f, %.0f)", relX, relZ);
        drawList->AddText(ImVec2(mousePos.x + 12, mousePos.y - 8), IM_COL32(200, 200, 200, 200), coordTxt);
    }

    drawList->PopClipRect();

    // Border
    drawList->AddRect(canvasPos, ImVec2(canvasPos.x + side, canvasPos.y + side),
                      IM_COL32(80, 80, 80, 255));

    ImGui::Dummy(ImVec2(side, side));

    // --- Stats ---
    ImGui::Separator();

    if (frustumCullingEnabled) {
        ImGui::TextColored(ImVec4(0.2f, 0.8f, 0.2f, 1.0f), "Visible: %d", visibleChunks);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.8f, 0.2f, 0.2f, 1.0f), "  Culled: %d", culledChunks);
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 0.4f, 0.4f, 1.0f), "  Empty: %d", emptyChunks);
        ImGui::Text("Draw calls saved: %d / %d (%.0f%%)",
                    culledChunks, totalChunks,
                    totalChunks > 0 ? (culledChunks * 100.0f / totalChunks) : 0.0f);
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.2f, 1.0f), "All %d chunks drawn (culling disabled)", totalChunks - emptyChunks);
        ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f), "Would cull: %d (%.0f%%)",
                           culledChunks, totalChunks > 0 ? (culledChunks * 100.0f / totalChunks) : 0.0f);
    }

    ImGui::Text("Player: (%.0f, %.0f, %.0f)", cameraPos.x, cameraPos.y, cameraPos.z);

    ImGui::End();
}
