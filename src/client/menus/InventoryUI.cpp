#include "InventoryUI.hpp"

#include <algorithm>

// Append the inventory icon for one item to the vertex stream. Routes to a
// flat sprite for vegetation/weapons/misc and to an isometric cube for full
// blocks, so blocks still look 3D and items look like 2D Minecraft items.
// Position arguments mirror the existing build2DInventoryCube call sites.
static void buildInventoryIcon(
	std::vector<float>& meshVertices,
	const ItemType& item,
	glm::vec2 origin,
	float scale,
	const TextureManager* texMgr)
{
	// Skip empty/sentinel block slots (BlockType::BEGIN == 0).
	if (auto* b = std::get_if<BlockType>(&item); b && *b == BlockType::BEGIN)
		return;

	// Torches are a block but, like Minecraft, show as a flat 2D icon
	bool flat = isItemFlat(item);
	if (auto* b = std::get_if<BlockType>(&item); b && isTorch(*b))
		flat = true;

	if (flat) {
		const int layer = texMgr ? texMgr->getItemSpriteLayer(item) : 0;
		build2DInventorySprite(meshVertices, origin, scale, layer);
	} else if (auto* b = std::get_if<BlockType>(&item)) {
		build2DInventoryCube(meshVertices, origin, scale, *b, texMgr);
	}
}

InventoryUI::InventoryUI(int width,
						int height,
						const TextureManager* texMgr,
						std::shared_ptr<PlayerInventory> playerInv,
						std::shared_ptr<CraftingStation> craftingStation,
						std::shared_ptr<InventoryExternalVariablesRefs> inventoryExternalVarsRefs) :

						Menu(width, height),
						textureManager(texMgr),
						playerInventory(playerInv),
						craftingStationInv(craftingStation)
{
	shader = std::make_unique<Shader>("shaders/InventoryCube.vert", "shaders/InventoryCube.frag"); //should probably reuse cubePropShader.frag

	if (inventoryExternalVarsRefs)
		handPtr = inventoryExternalVarsRefs->hand;

	inventoryRows = playerInv ? playerInv->getRows() : 4;
	inventoryCols = playerInv ? playerInv->getCols() : 9;
	craftingStationRows = craftingStation ? craftingStation->getRows() : 3;
	craftingStationCols = craftingStation ? craftingStation->getCols() : 3;

	MAX_BUFFER_SIZE = sizeof(float) * (inventoryRows * inventoryCols + craftingStationRows * craftingStationCols + 1 + 1) * (2 + 2 + 1) * 3 * 6; //2 coords, 2 uvs, 1 texLayer. 3 faces, 6 vertices. rows * cols + craftRows * craftCols + Hand + Result

	hotbarSlots.resize(inventoryCols);
	inventorySlots.resize(inventoryRows * inventoryCols);
	craftingStationSlots.resize(craftingStationRows * craftingStationCols);

	build();

	initGL();

	hotbarColor = glm::vec4(0.0f,0.0f,0.0f,0.5f);

	// videoPlayer = std::make_unique<VideoPlayer>("assets/videos/【東方】Bad Apple!! ＰＶ【影絵】 [FtutLA63Cp8].webm");

	badAppleTex = shader->loadTexture("assets/videos/badApple/frame_000262.png");
}

void InventoryUI::build()
{
	inventoryLayout.width = (menuWidth / 5.0f) * 3;
	inventoryLayout.height = menuHeight / 1.2f;
	inventoryLayout.y = (fullscreenHeight - inventoryLayout.height) / 2.0f;
	inventoryLayout.x = (fullscreenWidth - inventoryLayout.width) / 2.0f;

	inventory.x = inventoryLayout.x;
	inventory.y = inventoryLayout.y;
	inventory.width = inventoryLayout.width;
	inventory.height = inventoryLayout.height / 2.0f;

	hotbar.x = inventoryLayout.x;
	hotbar.y = menuHeight / 100.0f;
	hotbar.width = inventoryLayout.width;
	hotbar.height = inventory.height / 4.4f;

	for (int i = 0; i < inventoryCols ; i++)
	{
		hotbarSlots[i].width = hotbar.width / (inventoryCols + 1);
		hotbarSlots[i].height = hotbar.height - hotbar.height / 10.f;

		hotbarSlots[i].x = hotbar.x + (hotbarSlots[i].width / (inventoryCols + 1)) * (i + 1) + hotbarSlots[i].width * i;
		hotbarSlots[i].y = hotbar.y + (hotbar.height  - hotbarSlots[i].height) / 2.0f;
	}

	for (int i = 0; i < inventoryRows; i++)
	{
		for (int j = 0; j < inventoryCols; j++)
		{
			inventorySlots[i * inventoryCols + j].width = inventory.width / (inventoryCols + 1);
			inventorySlots[i * inventoryCols + j].height = inventory.height / (inventoryRows + 1);

			inventorySlots[i * inventoryCols + j].x = inventory.x + (inventorySlots[i * inventoryCols + j].width / (inventoryCols + 1)) * (j + 1) + inventorySlots[i * inventoryCols + j].width * j;
			inventorySlots[i * inventoryCols + j].y = inventory.y + (inventory.height / inventoryRows) * i + inventorySlots[i * inventoryCols + j].height / inventoryRows;
			if (i > 0)
				inventorySlots[i * inventoryCols + j].y += inventory.height / 15.0f;
		}
	}

	float padding = inventoryLayout.width * 0.02f;

	blackApple.x = hotbarSlots[0].x;
	blackApple.y = inventory.y + inventory.height + (inventory.height / 15.0f) * 2;
	blackApple.width = inventory.width / 2.35f;
	blackApple.height = inventory.height / 1.2f;

	craftingStation.x = inventoryLayout.x + blackApple.width + padding * 4;
	craftingStation.y = blackApple.y;
	craftingStation.height = blackApple.height;
	craftingStation.width = blackApple.height; // make it square

	for (int i = 0; i < craftingStationRows; i++)
	{
		for (int j = 0; j < craftingStationCols; j++)
		{
			int idx = (craftingStationRows - 1 - i) * craftingStationCols + j;

			float slotW = craftingStation.width  / (craftingStationCols + 1);
			float slotH = craftingStation.height / (craftingStationRows + 1);

			float gapX =
				(craftingStation.width - craftingStationCols * slotW)
				/ (craftingStationCols + 1);

			float gapY =
				(craftingStation.height - craftingStationRows * slotH)
				/ (craftingStationRows + 1);

			craftingStationSlots[idx].width  = slotW;
			craftingStationSlots[idx].height = slotH;

			craftingStationSlots[idx].x =
				craftingStation.x + gapX + j * (slotW + gapX);

			craftingStationSlots[idx].y =
				craftingStation.y + gapY + i * (slotH + gapY);
		}
	}

	craftingResultSlot.width  = hotbarSlots[0].width;
	craftingResultSlot.height = hotbarSlots[0].height;

	craftingResultSlot.x =
		craftingStation.x
	+ craftingStation.width
	+ padding;

	craftingResultSlot.y =
		craftingStation.y
	+ (craftingStation.height - craftingResultSlot.height) * 0.5f;

	// clamp inside inventory layout
	float maxX =
		inventoryLayout.x
	+ inventoryLayout.width
	- craftingResultSlot.width
	- padding;

	craftingResultSlot.x = std::min(craftingResultSlot.x, maxX);

	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);
	textRenderer.setScale(0.35f * menuScale);
}

InventoryUI::~InventoryUI()
{
	if (glfwGetCurrentContext()) {
		glDeleteVertexArrays(1, &inventoryTextureVAO);
		glDeleteBuffers(1, &inventoryTextureVBO);
		glDeleteTextures(1, &badAppleTex);
	} else {
		inventoryTextureVAO = 0;
		inventoryTextureVBO = 0;
		badAppleTex = 0;
	}
}

void InventoryUI::initGL()
{
	glGenVertexArrays(1, &inventoryTextureVAO);
	glGenBuffers(1, &inventoryTextureVBO);

	glBindVertexArray(inventoryTextureVAO);
	glBindBuffer(GL_ARRAY_BUFFER, inventoryTextureVBO);

	glBufferData(
		GL_ARRAY_BUFFER,
		MAX_BUFFER_SIZE,
		nullptr,
		GL_DYNAMIC_DRAW
	);

	// position
	glVertexAttribPointer(
		0, 2, GL_FLOAT, GL_FALSE,
		5 * sizeof(float),
		(void*)0
	);
	glEnableVertexAttribArray(0);

	// uv
	glVertexAttribPointer(
		1, 2, GL_FLOAT, GL_FALSE,
		5 * sizeof(float),
		(void*)(2 * sizeof(float))
	);
	glEnableVertexAttribArray(1);

	// texture layer
	glVertexAttribPointer(
		2, 1, GL_FLOAT, GL_FALSE,
		5 * sizeof(float),
		(void*)(4 * sizeof(float))
	);
	glEnableVertexAttribArray(2);

	glBindVertexArray(0);
}

void InventoryUI::setupCubes(const std::vector<float> &meshVertices)
{
	shader->use();

	glBindVertexArray(inventoryTextureVAO);

	shader->setInt("atlas", 0);
	shader->setVec2("uScreenSize", glm::vec2(fullscreenWidth, fullscreenHeight));

	// texture array
	if (textureManager)
		textureManager->bind(GL_TEXTURE0);

	glBindBuffer(GL_ARRAY_BUFFER, inventoryTextureVBO);
	glBufferSubData(
		GL_ARRAY_BUFFER,
		0,
		meshVertices.size() * sizeof(float),
		meshVertices.data()
	);

	// draw
	glDrawArrays(GL_TRIANGLES, 0, meshVertices.size() / 5);
}

void InventoryUI::drawHotbar()
{
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	std::vector<float> meshVertices;

	std::shared_ptr<PlayerInventory> inv = playerInventory.lock();
	if (!inv)
		return ;

	uint8_t i = 0;
	for (auto &hotbarSlotCoord : hotbarSlots)
	{
		drawSimpleQuad(hotbarSlotCoord.x, hotbarSlotCoord.y, hotbarSlotCoord.width, hotbarSlotCoord.height, hotbarColor);
		i++;
	}

	{
		uint8_t sel = inv->activeHotbarSlot;
		if (sel < hotbarSlots.size())
		{
			const auto& s = hotbarSlots[sel];
			// Clamp both so the outer frame is never thinner than the inner
			// separator, and cap by half the slot so neither draws past it.
			float maxT   = 0.5f * std::min(s.width, s.height);
			float t      = std::min(maxT, std::max(1.0f, 2.0f * menuScale)); // outer bright border
			float tInner = std::min(t,    std::max(1.0f, 1.0f * menuScale)); // inner dark separator
			glm::vec4 bright(1.0f, 1.0f, 1.0f, 1.0f);
			glm::vec4 shadow(0.0f, 0.0f, 0.0f, 0.55f);

			// Outer bright frame (top / bottom / left / right strips around the slot)
			drawSimpleQuad(s.x - t,           s.y + s.height,    s.width + 2 * t, t,           bright);
			drawSimpleQuad(s.x - t,           s.y - t,           s.width + 2 * t, t,           bright);
			drawSimpleQuad(s.x - t,           s.y,               t,               s.height,    bright);
			drawSimpleQuad(s.x + s.width,     s.y,               t,               s.height,    bright);

			drawSimpleQuad(s.x,               s.y + s.height - tInner, s.width,    tInner,     shadow);
			drawSimpleQuad(s.x,               s.y,                     s.width,    tInner,     shadow);
			drawSimpleQuad(s.x,               s.y,                     tInner,     s.height,   shadow);
			drawSimpleQuad(s.x + s.width - tInner, s.y,                tInner,     s.height,   shadow);
		}
	}

	i = 0;
	for (auto &hotbarSlotCoord : hotbarSlots)
	{
		textRenderer.renderText(std::to_string(inv->getSlot(i).second), hotbarSlotCoord.x + 3 * menuScale, hotbarSlotCoord.y + hotbar.height * 0.7, glm::vec3(1.0f));

		//could optimize and only redo if inventory/hotbar has changed. TODO ?
		buildInventoryIcon(meshVertices, inv->getItemAtSlot(i),
			glm::vec2(hotbarSlotCoord.x + 18 * menuScale, hotbarSlotCoord.y + 5 * menuScale),
			40 * menuScale, textureManager);
		i++;
	}

	setupCubes(meshVertices);

	glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

InventoryType InventoryUI::getCurrentInventoryType(double mouseX, double mouseY) const
{
	mouseY = fullscreenHeight - mouseY;
	//Crafting Station is inside of InventoryLayout so it should go first
	if (mouseX >= craftingStation.x && mouseX <= craftingStation.x + craftingStation.width &&
		mouseY >= craftingStation.y && mouseY <= craftingStation.y + craftingStation.height)
		return InventoryType::CRAFTING_STATION;
	//Result slot is outside of the station layout but it's still part of it
	if (mouseX >= craftingResultSlot.x && mouseX <= craftingResultSlot.x + craftingResultSlot.width &&
		mouseY >= craftingResultSlot.y && mouseY <= craftingResultSlot.y + craftingResultSlot.height)
		return InventoryType::CRAFTING_STATION;

	if (mouseX >= inventoryLayout.x && mouseX <= inventoryLayout.x + inventoryLayout.width &&
		mouseY >= inventoryLayout.y && mouseY <= inventoryLayout.y + inventoryLayout.height)
		return InventoryType::PLAYER;
	return InventoryType::NONE;
}

int InventoryUI::getCraftingSlotAt(double mouseX, double mouseY) const
{
	mouseY = fullscreenHeight - mouseY;
	for (int i = 0; i < craftingStationRows * craftingStationCols; ++i)
	{
		const auto& s = craftingStationSlots[i];
		if (mouseX >= s.x && mouseX <= s.x + s.width &&
			mouseY >= s.y && mouseY <= s.y + s.height)
			return i;
	}
	if (mouseX >= craftingResultSlot.x && mouseX <= craftingResultSlot.x + craftingResultSlot.width &&
		mouseY >= craftingResultSlot.y && mouseY <= craftingResultSlot.y + craftingResultSlot.height)
		return craftingStationInv.lock() ? craftingStationInv.lock()->getResultSlotID() : -1;

	return -1;
}

void InventoryUI::drawHealth(float health) const
{
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	float offset = hotbarSlots[0].width / (inventoryCols + 1);
	float HPBarLenght = (health / 20.0f) * (hotbar.width) - offset * 2;
	drawSimpleQuad(hotbar.x + offset, hotbar.y + hotbar.height + 10, HPBarLenght, hotbar.height / 5.0f,
		glm::vec4(
			210/255.0f,
			35/255.0f,
			25/255.0f,
			1.0f
		));

	drawSimpleQuad(hotbar.x + offset, hotbar.y + hotbar.height + 10, hotbar.width - offset * 2, hotbar.height / 5.0f, glm::vec4(0,0,0,0.4f));

	glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void InventoryUI::drawCrosshair() const {

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// draw cross hair. Eventually might want to be a texture. And not in "drawHealth"
	drawSimpleQuad(
		fullscreenWidth / 2.0f - 10,
		fullscreenHeight / 2.0f - 1,
		20,
		2,
		glm::vec4(60/255.0f,60/255.0f,60/255.0f,0.6)
	);
	drawSimpleQuad(
		fullscreenWidth / 2.0f - 1,
		fullscreenHeight / 2.0f - 10,
		2,
		20,
		glm::vec4(60/255.0f,60/255.0f,60/255.0f,0.6)
	);

	glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

// Death overlay: red vignette + "YOU DIED" title that fades in once health
// hits 0 and disappears the moment the server respawns us. Server keeps us
// dead for ~3s (DEATH_ANIMATION_TICKS in sendDeaths)
void InventoryUI::drawDeathScreen(float health)
{
	const double now = glfwGetTime();

	if (health > 0.0f) {
		// Alive (or just respawned): clear state so the next death restarts
		// the fade-in from zero.
		deathStartTime = -1.0;
		return;
	}

	if (deathStartTime < 0.0)
		deathStartTime = now;

	const float elapsed = static_cast<float>(now - deathStartTime);

	// Fade-in over ~0.45s, capped slightly below 1 so the world still bleeds
	// through and it doesn't feel like we slammed a menu over the screen.
	const float fade = std::min(1.0f, elapsed / 0.45f);

	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	// Base red wash. Deep crimson so it reads as "you got hurt" rather than
	// a generic UI panel.
	drawSimpleQuad(0, 0, fullscreenWidth, fullscreenHeight,
		glm::vec4(0.55f, 0.04f, 0.04f, 0.55f * fade));

	// Cheap vignette: stack a few darkening bands at the top/bottom that
	// fake a radial falloff without a custom shader. The middle stays mostly
	// red while the edges go nearly black.
	const float bandH = fullscreenHeight * 0.18f;
	for (int i = 0; i < 4; ++i)
	{
		const float t = (i + 1) / 4.0f; // 0.25..1.0 — outer bands are darker
		const float a = 0.18f * t * fade;
		drawSimpleQuad(0, fullscreenHeight - bandH * (i + 1),
			fullscreenWidth, bandH, glm::vec4(0.0f, 0.0f, 0.0f, a));
		drawSimpleQuad(0, bandH * i,
			fullscreenWidth, bandH, glm::vec4(0.0f, 0.0f, 0.0f, a));
	}

	// A slow heartbeat-style pulse on top
	const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(now) * 2.2f);
	drawSimpleQuad(0, 0, fullscreenWidth, fullscreenHeight,
		glm::vec4(0.4f, 0.0f, 0.0f, 0.08f * pulse * fade));

	// ---- "YOU DIED" title --------------------------------------------------
	// Use the titleRenderer (128px font) scaled by menuScale so it looks the
	// same on every resolution.
	const std::string title = "YOU DIED";
	const float savedTitleScale = titleRenderer.getScale();
	const float titleScale = 1.1f * menuScale;
	titleRenderer.setScale(titleScale);
	titleRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	// Slight overshoot/settle: title scales up a touch in the first 0.35s,
	// then locks in.
	const float titleWidth = titleRenderer.getPixelSizeOfString(title);
	const float titleX = (fullscreenWidth - titleWidth) / 2.0f;
	const float titleY = fullscreenHeight * 0.58f;

	// Drop shadow for legibility against the red wash.
	const float shadowOff = 4.0f * menuScale;
	titleRenderer.renderText(title, titleX + shadowOff, titleY - shadowOff,
		glm::vec3(0.0f), fade);
	// Main title in a slightly warm off-white so it doesn't fight the red.
	titleRenderer.renderText(title, titleX, titleY,
		glm::vec3(0.96f, 0.88f, 0.82f), fade);
	titleRenderer.setScale(savedTitleScale);

	// ---- Subtitle "Respawning…" -------------------------------------------
	const float savedTextScale = textRenderer.getScale();
	const float subScale = 0.45f * menuScale;
	textRenderer.setScale(subScale);
	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	// Animated ellipsis
	const int dotCount = static_cast<int>(now * 2.0) % 4; // 0..3
	std::string sub = "Respawning";
	for (int i = 0; i < dotCount; ++i) sub += '.';

	const float subWidth = textRenderer.getPixelSizeOfString(sub);
	const float subX = (fullscreenWidth - subWidth) / 2.0f;
	const float subY = fullscreenHeight * 0.50f;
	// Subtle alpha pulse on the subtitle in sync with the heartbeat.
	const float subAlpha = (0.6f + 0.4f * pulse) * fade;
	textRenderer.renderText(sub, subX, subY,
		glm::vec3(0.85f, 0.20f, 0.20f), subAlpha);

	// Flavor line near the bottom. Kept short and rotated through a tiny
	// pool so death isn't always the same screen.
	static const char* const flavor[] = {
		"the world claims another soul.",
		"that hurt.",
		"see you on the other side.",
		"gravity: undefeated.",
		"better luck next life.",
	};
	constexpr int flavorCount = static_cast<int>(sizeof(flavor) / sizeof(flavor[0]));
	// Pick a line per death (stable for the duration of this death screen).
	const int idx = static_cast<int>(deathStartTime * 7.0) % flavorCount;
	const std::string& line = flavor[(idx + flavorCount) % flavorCount];

	const float flavorScale = 0.35f * menuScale;
	textRenderer.setScale(flavorScale);
	const float flavorWidth = textRenderer.getPixelSizeOfString(line);
	textRenderer.renderText(line,
		(fullscreenWidth - flavorWidth) / 2.0f,
		fullscreenHeight * 0.40f,
		glm::vec3(0.7f, 0.5f, 0.5f), 0.85f * fade);
	textRenderer.setScale(savedTextScale);

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
}

int InventoryUI::getSlotAt(double mouseX, double mouseY) const
{
	mouseY = fullscreenHeight - mouseY;
    for (int i = 0; i < inventoryRows * inventoryCols; ++i)
    {
        const auto& s = inventorySlots[i];
        if (mouseX >= s.x && mouseX <= s.x + s.width &&
            mouseY >= s.y && mouseY <= s.y + s.height)
            return i;
    }
	return -1;
}

//add a slot to the drag if mouse is on a different slot
bool InventoryUI::checkInventoryDrag(NetInventoryAction &pkt)
{
	if (!dragging)
		return false;

	int currHoveredSlot = -1;
	InventoryType inventoryType = getCurrentInventoryType(mouseX, mouseY);

	if (inventoryType == InventoryType::PLAYER)
	{
		int slot = getSlotAt(mouseX, mouseY);
		if (slot != -1)
			currHoveredSlot = slot;
	}
	else if (inventoryType == InventoryType::CRAFTING_STATION)
	{
		int slot = getCraftingSlotAt(mouseX, mouseY);
		if (slot != -1)
			currHoveredSlot = slot;
	}

	if (currHoveredSlot != -1 && currHoveredSlot != lastHoveredSlot)
	{
		pkt.actionType = dragButton;
		pkt.inventoryTypeID = static_cast<uint8_t>(inventoryType);
		pkt.modifier = InventoryModifiers::INV_DRAG_ADD;
		pkt.slot = currHoveredSlot;

		lastHoveredSlot = currHoveredSlot;
		return true;
	}

	return false;
}

void InventoryUI::handleInventoryModifiers(NetInventoryAction &pkt, int action, int button)
{
	static float lastClickTime = -1.0f;

	auto setDragToFalse = [this]()
	{
		dragging = false;
		dragButton = -1;
	};

	if (action == GLFW_PRESS && button == GLFW_MOUSE_BUTTON_LEFT) //if we double clicked in less than 0.5 seconds
	{
		float prevLastClickTime = lastClickTime;
		lastClickTime = glfwGetTime();

		if (lastClickTime - prevLastClickTime <= 0.2) //double click triggers with 2 clicks in less than 0.2 seconds
		{
			pkt.modifier = InventoryModifiers::INV_DOUBLE_CLICK;
			setDragToFalse();
			return ;
		}
	}

	if (dragging && action == GLFW_PRESS && button != dragButton)
	{
		pkt.modifier = InventoryModifiers::INV_DRAG_CANCEL;
		setDragToFalse();
		return ;
	}

	// we start dragging
	if (handPtr.lock() && handPtr.lock()->second != 0 && action == GLFW_PRESS && !dragging)
	{
		pkt.modifier = InventoryModifiers::INV_DRAG_BEGIN;
		dragging = true;
		dragButton = button;
	}
	// we add to the drag selection
	else if (handPtr.lock() && handPtr.lock()->second == 0 && action == GLFW_PRESS && dragging)
	{
		pkt.modifier = InventoryModifiers::INV_DRAG_ADD;
	}
	// we end dragging
	else if (button == dragButton && dragging && action == GLFW_RELEASE)
	{
		pkt.modifier = InventoryModifiers::INV_DRAG_END;
		pkt.actionType = button == GLFW_MOUSE_BUTTON_LEFT ? InventoryActionType::INV_LEFT_CLICK : InventoryActionType::INV_RIGHT_CLICK;
		setDragToFalse();
	}
}

void InventoryUI::handleMouseClick(double mouseX, double mouseY, int button, int action)
{
	InventoryType inventoryType = getCurrentInventoryType(mouseX, mouseY);
	if (inventoryType == InventoryType::NONE)
		return ;
    if (action != GLFW_PRESS && action != GLFW_RELEASE) return ;

	this->mouseX = mouseX;
	this->mouseY = mouseY;

	NetInventoryAction pkt;

	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) //left click
		pkt.actionType = InventoryActionType::INV_LEFT_CLICK;
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS) //right click
		pkt.actionType = InventoryActionType::INV_RIGHT_CLICK;
	pkt.inventoryTypeID = static_cast<uint8_t>(inventoryType);

	handleInventoryModifiers(pkt, action, button);

	if (inventoryType == InventoryType::PLAYER)
	{
		int slot = getSlotAt(mouseX, mouseY);
		if (slot != -1)
		{
			pkt.slot = slot;
			lastAction.emplace(pkt);
			lastHoveredSlot = slot;
		}
	}

	else if (inventoryType == InventoryType::CRAFTING_STATION)
	{
		int slot = getCraftingSlotAt(mouseX, mouseY);
		if (!craftingStationInv.lock()) return ;
		if (slot == craftingStationInv.lock()->getResultSlotID() && action == GLFW_RELEASE) return ;

		if (slot != -1)
		{
			pkt.slot = slot;
			lastAction.emplace(pkt);
			lastHoveredSlot = slot;
		}
	}
}

void InventoryUI::handleMouseMove(double mouseX, double mouseY)
{
	this->mouseX = mouseX;
	this->mouseY = mouseY;
}

void InventoryUI::drawEveryInventoryQuad()
{
	drawSimpleQuad(
		inventoryLayout.x,
		inventoryLayout.y,
		inventoryLayout.width,
		inventoryLayout.height,
		glm::vec4(160/255.0f, 160/255.0f, 160/255.0f, 1)
	);

	// "inventory.height / 15.0f" is the padding between rows. Used as if it was a scalar
	for (int i = 0; i < inventoryRows * inventoryCols; i++)
	{
		drawSimpleQuad(
			inventorySlots[i].x,
			inventorySlots[i].y,
			inventorySlots[i].width,
			inventorySlots[i].height,
			glm::vec4(0,0,0,0.4f)
		);
	}

	drawSimpleQuad(
		blackApple.x,
		blackApple.y,
		blackApple.width,
		blackApple.height,
		glm::vec4(0,0,0,1.0f)
	);

	// if (videoPlayer->nextFrame()) {
	// 	drawTexturedQuad(
	// 		blackApple.x,
	// 		blackApple.y,
	// 		blackApple.width,
	// 		blackApple.height,
	// 		videoPlayer->getTexture()
	// 	);
	// }

	drawTexturedQuad(
		blackApple.x,
		blackApple.y,
		blackApple.width,
		blackApple.height,
		badAppleTex
	);

	//crafting station UI, those 3 should be replaced when the crafting staion class gets created
	for (int i = 0; i < craftingStationRows * craftingStationCols; i++)
	{
		drawSimpleQuad(
			craftingStationSlots[i].x,
			craftingStationSlots[i].y,
			craftingStationSlots[i].width,
			craftingStationSlots[i].height,
			glm::vec4(0,0,0,1.0f)
		);
	}

	drawSimpleQuad(
		craftingStation.x,
		craftingStation.y,
		craftingStation.width,
		craftingStation.height,
		glm::vec4(0,0,0,0.4f)
	);

	drawSimpleQuad(
		craftingResultSlot.x,
		craftingResultSlot.y,
		craftingResultSlot.width,
		craftingResultSlot.height,
		glm::vec4(0,0,0,1.0f)
	);
}

void InventoryUI::onRender()
{
	drawEveryInventoryQuad();
	std::vector<float> meshVertices;

	//inventory cubes/text
	for (int i = 0; i < inventoryRows * inventoryCols; i++)
	{
		std::shared_ptr<PlayerInventory> inv = playerInventory.lock();
		if (!inv)
			return ;

		if (inv->getSlot(i).second)
			textRenderer.renderText(std::to_string(inv->getSlot(i).second), inventorySlots[i].x, inventorySlots[i].y + hotbar.height * 0.7, glm::vec3(1.0f));

		//could optimize and only redo if inventory/hotbar has changed. TODO ?
		buildInventoryIcon(meshVertices, inv->getItemAtSlot(i),
			glm::vec2(inventorySlots[i].x + 18 * menuScale, inventorySlots[i].y + 5 * menuScale),
			40 * menuScale, textureManager);
	}

	//crafting station cubes/text
	for (int i = 0; i < craftingStationRows * craftingStationCols; i++)
	{
		std::shared_ptr<CraftingStation> inv = craftingStationInv.lock();
		if (!inv)
			return ;

		if (inv->getSlot(i).second)
			textRenderer.renderText(std::to_string(inv->getSlot(i).second), craftingStationSlots[i].x, craftingStationSlots[i].y + hotbar.height * 0.7, glm::vec3(1.0f));

		//could optimize and only redo if inventory/hotbar has changed. TODO ?
		buildInventoryIcon(meshVertices, inv->getItemAtSlot(i),
			glm::vec2(craftingStationSlots[i].x + 11 * menuScale, craftingStationSlots[i].y + 5 * menuScale),
			40 * menuScale, textureManager);
	}

	if (craftingStationInv.lock())
	{
		std::shared_ptr<CraftingStation> inv = craftingStationInv.lock();
		if (!inv)
			return ;
		
		if (inv->getSlot(inv->getResultSlotID()).second)
			textRenderer.renderText(std::to_string(inv->getSlot(inv->getResultSlotID()).second), craftingResultSlot.x, craftingResultSlot.y + hotbar.height * 0.7, glm::vec3(1.0f));

		buildInventoryIcon(meshVertices,
			inv->getSlot(inv->getResultSlotID()).first,
			glm::vec2(craftingResultSlot.x + 18 * menuScale, craftingResultSlot.y + 5 * menuScale),
			40 * menuScale, textureManager);
	}

	//cube in hand
	if (handPtr.lock() && handPtr.lock()->second != 0)
	{
		buildInventoryIcon(meshVertices, handPtr.lock()->first,
			glm::vec2(mouseX - 10 * menuScale, fullscreenHeight - mouseY - 10 * menuScale),
			40 * menuScale, textureManager);
	}

	setupCubes(meshVertices);

	//text in hand
	//Text needs to go after setupCubes so it renders in front of the cube in hand.
	if (handPtr.lock() && handPtr.lock()->second != 0)
		textRenderer.renderText(std::to_string(handPtr.lock()->second), mouseX - hotbarSlots[0].width / 4.0f , fullscreenHeight - mouseY + hotbar.height * 0.4, glm::vec3(1.0f));
}