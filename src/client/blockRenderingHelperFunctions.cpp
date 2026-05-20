
#include "blockRenderingHelperFunctions.hpp"
#include "TextureManager.hpp"

void buildCube(
	std::vector<float>& meshVertices,
	float x, float y, float z,
	int originX, int originZ,
	BlockType type,
	const TextureManager* texMgr,
	bool isIlluminated,
	float skyFactor,
	float blockFactor)
{
	addFace(meshVertices, x, y, z, originX, originZ, type, 0, texMgr, isIlluminated, skyFactor, blockFactor);
	addFace(meshVertices, x, y, z, originX, originZ, type, 1, texMgr, isIlluminated, skyFactor, blockFactor);
	addFace(meshVertices, x, y, z, originX, originZ, type, 2, texMgr, isIlluminated, skyFactor, blockFactor);
	addFace(meshVertices, x, y, z, originX, originZ, type, 3, texMgr, isIlluminated, skyFactor, blockFactor);
	addFace(meshVertices, x, y, z, originX, originZ, type, 4, texMgr, isIlluminated, skyFactor, blockFactor);
	addFace(meshVertices, x, y, z, originX, originZ, type, 5, texMgr, isIlluminated, skyFactor, blockFactor);
}

void build2DInventoryCube(
	std::vector<float>& meshVertices,
	glm::vec2 origin,
	float scale,
	BlockType type,
	const TextureManager* texMgr)
{
	addInventoryFace(meshVertices, origin, scale, type, 2, texMgr);
	addInventoryFace(meshVertices, origin, scale, type, 1, texMgr);
	addInventoryFace(meshVertices, origin, scale, type, 0, texMgr);
}

void addInventoryFace(
	std::vector<float>& meshVertices,
	glm::vec2 origin,				// inventory slot position
	float scale,
	BlockType type,
	int face,
	const TextureManager* texMgr)
{
    constexpr int quadToTri[6] = { 0,1,2, 2,3,0 };

    // Map inventory face index to block face index for texture lookup:
    // inventory face 0 = BOTTOM LEFT = block face 5 (LEFT/-X)
    // inventory face 1 = BOTTOM RIGHT = block face 4 (RIGHT/+X)
    // inventory face 2 = TOP = block face 2 (TOP/+Y)
    static const int invFaceToBlockFace[3] = { 5, 4, 2 };
    int blockFace = invFaceToBlockFace[face];

    float texLayer = 0.0f;
    if (texMgr) {
        const BlockTextures& bt = texMgr->getBlockTextures(type);
        texLayer = static_cast<float>(bt.getLayerForFace(blockFace));
    }

    for (int i = 0; i < 6; ++i)
    {
        int v = quadToTri[i];

        glm::vec2 basePos = unitFacePositionsInventory[face][v];
        glm::vec2 pos = origin + basePos * scale;

		// Cactus: inset side faces for inventory rendering (faces 0 and 1 are sides)
		if (type == BlockType::CACTUS && (face == 0 || face == 1)) {
			constexpr float cactusInset = 1.0f / 16.0f;
			// Shift the face inward on the horizontal axis
			if (face == 0) { // left face
				pos.x += cactusInset * scale;
			} else { // right face
				pos.x -= cactusInset * scale;
			}
		}

        glm::vec2 uv = {
            uvTemplate[i].x,
            uvTemplate[i].y
        };

        meshVertices.push_back(pos.x);
        meshVertices.push_back(pos.y);
        meshVertices.push_back(uv.x);
        meshVertices.push_back(uv.y);
        meshVertices.push_back(texLayer);
    }
}

void buildItemSprite(
	std::vector<float>& meshVertices,
	float x, float y, float z,
	int texLayer,
	float skyFactor,
	float blockFactor)
{
	// Two perpendicular quads forming an X (cross-pattern), centered on the
	// drop position. Same look as world vegetation so the dropped item reads
	// as the 2D thing it is. Y goes 0..h (item floats slightly above its
	// origin like the dropped cube does).
	constexpr float w = 0.125f;   // half-extent on the horizontal diagonal
	constexpr float h = 0.25f;    // sprite height
	const float layer = static_cast<float>(texLayer);

	// quad triangulation order
	auto pushVert = [&](float px, float py, float pz, float u, float v) {
		meshVertices.push_back(x + px);
		meshVertices.push_back(y + py);
		meshVertices.push_back(z + pz);
		meshVertices.push_back(u);
		meshVertices.push_back(v);
		meshVertices.push_back(layer);
		meshVertices.push_back(skyFactor);
		meshVertices.push_back(blockFactor);
	};

	// UV convention matches addFace: V=0 at the geometry bottom, V=1 at the
	// top, so the texture appears right-side up.
	// Quad 1: NW-SE diagonal (-w,-w) → (+w,+w)
	pushVert(-w, 0.0f, -w, 0.0f, 0.0f);
	pushVert( w, 0.0f,  w, 1.0f, 0.0f);
	pushVert( w, h,     w, 1.0f, 1.0f);
	pushVert( w, h,     w, 1.0f, 1.0f);
	pushVert(-w, h,    -w, 0.0f, 1.0f);
	pushVert(-w, 0.0f, -w, 0.0f, 0.0f);

	// Quad 2: NE-SW diagonal (+w,-w) → (-w,+w)
	pushVert( w, 0.0f, -w, 0.0f, 0.0f);
	pushVert(-w, 0.0f,  w, 1.0f, 0.0f);
	pushVert(-w, h,     w, 1.0f, 1.0f);
	pushVert(-w, h,     w, 1.0f, 1.0f);
	pushVert( w, h,    -w, 0.0f, 1.0f);
	pushVert( w, 0.0f, -w, 0.0f, 0.0f);

	// ItemPropEntityManager packs 36 verts per item (6 floats each); the draw
	// call is `glDrawArrays(GL_TRIANGLES, 0, entities.size() * 36)`. Pad the
	// remaining 24 verts with a degenerate triangle (all three corners equal)
	// so the GPU rejects them and emits no fragments.
	for (int i = 0; i < 24; ++i) {
		pushVert(0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
	}
}

void buildItemBillboard(
	std::vector<float>& meshVertices,
	const glm::dvec3& itemRel,
	int texLayer,
	float skyFactor,
	float blockFactor)
{
	// Y-axis billboard: the quad is upright (world up) and rotates around Y
	// to face the camera horizontally. Doesn't tilt when the player looks up
	// or down
	constexpr float halfW = 0.125f;   // half-width of the sprite
	constexpr float h     = 0.25f;    // total height of the sprite
	const float layer = static_cast<float>(texLayer);

	// "To-camera" direction projected onto the XZ plane, in double precision
	// so far-away items still get a stable right vector.
	glm::dvec3 toCam(-itemRel.x, 0.0, -itemRel.z);
	const double lenSq = toCam.x * toCam.x + toCam.z * toCam.z;
	glm::dvec3 rightD;
	if (lenSq < 1e-9) {
		// Item is directly above/below the eye — pick an arbitrary right.
		rightD = glm::dvec3(1.0, 0.0, 0.0);
	} else {
		toCam /= std::sqrt(lenSq);
		// right = up × toCam keeps the quad's front facing the camera.
		rightD = glm::cross(glm::dvec3(0.0, 1.0, 0.0), toCam);
	}
	const glm::vec3 right = glm::vec3(rightD);
	const glm::vec3 up    = glm::vec3(0.0f, 1.0f, 0.0f);

	const glm::vec3 c(static_cast<float>(itemRel.x),
	                  static_cast<float>(itemRel.y),
	                  static_cast<float>(itemRel.z));

	// Corner positions: bottom-left, bottom-right, top-right, top-left.
	// Sprite sits with its base at the item's origin
	const glm::vec3 bl = c - right * halfW;
	const glm::vec3 br = c + right * halfW;
	const glm::vec3 tr = c + right * halfW + up * h;
	const glm::vec3 tl = c - right * halfW + up * h;

	auto pushVert = [&](const glm::vec3& p, float u, float v) {
		meshVertices.push_back(p.x);
		meshVertices.push_back(p.y);
		meshVertices.push_back(p.z);
		meshVertices.push_back(u);
		meshVertices.push_back(v);
		meshVertices.push_back(layer);
		meshVertices.push_back(skyFactor);
		meshVertices.push_back(blockFactor);
	};

	// Two triangles forming the quad. UV convention matches buildItemSprite:
	// V=0 at the bottom, V=1 at the top.
	pushVert(bl, 0.0f, 0.0f);
	pushVert(br, 1.0f, 0.0f);
	pushVert(tr, 1.0f, 1.0f);
	pushVert(tr, 1.0f, 1.0f);
	pushVert(tl, 0.0f, 1.0f);
	pushVert(bl, 0.0f, 0.0f);

	// Pad to the 36-vert slot the manager allocates per item.
	for (int i = 0; i < 30; ++i) {
		pushVert(bl, 0.0f, 0.0f);
	}
}

void build2DInventorySprite(
	std::vector<float>& meshVertices,
	glm::vec2 origin,
	float scale,
	int texLayer)
{
	// One flat textured quad in screen space. Format matches addInventoryFace:
	// pos(2) + uv(2) + texLayer(1) per vertex, 6 verts (two triangles).
	const float layer = static_cast<float>(texLayer);

	const glm::vec2 p[4] = {
		origin,                               // bottom-left
		origin + glm::vec2(scale, 0.0f),      // bottom-right
		origin + glm::vec2(scale, scale),     // top-right
		origin + glm::vec2(0.0f,  scale),     // top-left
	};
	// UV convention matches addFace/uvTemplate: V=0 at the geometry bottom,
	// V=1 at the top. Inventory screen-space has Y growing upward, so
	// p[0..3] = BL, BR, TR, TL get V = 0, 0, 1, 1.
	const glm::vec2 uv[4] = {
		{0.0f, 0.0f}, {1.0f, 0.0f}, {1.0f, 1.0f}, {0.0f, 1.0f}
	};

	auto pushVert = [&](int i) {
		meshVertices.push_back(p[i].x);
		meshVertices.push_back(p[i].y);
		meshVertices.push_back(uv[i].x);
		meshVertices.push_back(uv[i].y);
		meshVertices.push_back(layer);
	};

	// Two triangles: 0,1,2 and 2,3,0
	pushVert(0); pushVert(1); pushVert(2);
	pushVert(2); pushVert(3); pushVert(0);
}

void addFace(
	std::vector<float>& meshVertices,
    float x, float y, float z,
    int originX, int originZ,
    BlockType type, int face,
    const TextureManager* texMgr,
    bool isIlluminated,
    float skyFactor,
    float blockFactor)
{
    glm::vec3 normal = faceNormals[face];

    // Get texture layer for this block face from TextureManager
    float texLayer = 0.0f;
    if (texMgr) {
        const BlockTextures& bt = texMgr->getBlockTextures(type);
        texLayer = static_cast<float>(bt.getLayerForFace(face));
    }
    
    for (int i = 0; i < 6; ++i) {
        glm::vec3 basePos = unitFacePositions[face][i];

        // world aligned [0..1] + block origin
        glm::vec3 pos = { originX + x + basePos.x,
                    y + basePos.y,
                    originZ + z + basePos.z };
        if (!isIlluminated) //if it's itemprop..
		{
			constexpr float scale = 0.2f;
			constexpr float cactusInset = 1.0f / 16.0f;

			// Cactus: inset side faces by 1/16 of a block (same as placed block)
			glm::vec3 adjustedBasePos = basePos;
			bool isCactusSide = (type == BlockType::CACTUS && face != 2 && face != 3);
			if (isCactusSide) {
				if (face == 0) adjustedBasePos.z = 1.0f - cactusInset;   // front (Z+): pull inward
				if (face == 1) adjustedBasePos.z = cactusInset;           // back  (Z-): push inward
				if (face == 4) adjustedBasePos.x = 1.0f - cactusInset;   // right (X+): pull inward
				if (face == 5) adjustedBasePos.x = cactusInset;           // left  (X-): push inward
			}

			// Translate vertex so (0.5, 0.5, 0.5) becomes the origin, scale, then translate back
			pos = glm::vec3(originX + x, y, originZ + z)
				+ glm::vec3(adjustedBasePos - 0.5f) * scale
				+ glm::vec3(0.0f, 0.5f * scale, 0.0f);
		}

        glm::vec2 uv = {
            uvTemplate[i].x,
            uvTemplate[i].y
        };

        // Pack vertex attributes
        meshVertices.push_back(pos.x);
        meshVertices.push_back(pos.y);
        meshVertices.push_back(pos.z);
        meshVertices.push_back(uv.x);
        meshVertices.push_back(uv.y);
        meshVertices.push_back(texLayer);

        if (isIlluminated) {
            meshVertices.push_back(pos.y);     // gradient Y
            meshVertices.push_back(normal.x);
            meshVertices.push_back(normal.y);
            meshVertices.push_back(normal.z);
        } else {
            // Dropped-item path: per-vertex sky-light + baked torch
            // block-light. Both sampled once at the item position by the host.
            meshVertices.push_back(skyFactor);
            meshVertices.push_back(blockFactor);
        }
    }
}
