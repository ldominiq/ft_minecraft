#include "InventoryUI.hpp"

constexpr float textScale = 0.3f;

InventoryUI::InventoryUI(int width, int height, const TextureManager* texMgr): Inventory(), Menu(width, height), textureManager(texMgr)
{
	shader = std::make_unique<Shader>("shaders/InventoryCube.vert", "shaders/InventoryCube.frag"); //should probably reuse cubePropShader.frag

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

	for (int i = 0; i < MAX_SLOTS ; i++)
	{
		hotbarSlots[i].width = hotbar.width / (MAX_SLOTS + 1);
		hotbarSlots[i].height = hotbar.height - hotbar.height / 10.f;

		hotbarSlots[i].x = hotbar.x + (hotbarSlots[i].width / (MAX_SLOTS + 1)) * (i + 1) + hotbarSlots[i].width * i;
		hotbarSlots[i].y = hotbar.y + (hotbar.height  - hotbarSlots[i].height) / 2.0f;
	}

	for (int i = 0; i < rows; i++)
	{
		for (int j = 0; j < cols; j++)
		{
			inventorySlots[i * cols + j].width = inventory.width / (cols + 1);
			inventorySlots[i * cols + j].height = inventory.height / (rows + 1);

			inventorySlots[i * cols + j].x = inventory.x + (inventorySlots[i * cols + j].width / (cols + 1)) * (j + 1) + inventorySlots[i * cols + j].width * j;
			inventorySlots[i * cols + j].y = inventory.y + (inventory.height / rows) * i + inventorySlots[i * cols + j].height / rows;
			if (i > 0)
				inventorySlots[i * cols + j].y += inventory.height / 15.0f;
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

	for (int i = 0; i < MAX_CRAFTING_SLOTS; i++)
	{
		for (int j = 0; j < MAX_CRAFTING_SLOTS; j++)
		{
			int idx = i * MAX_CRAFTING_SLOTS + j;

			float slotW = craftingStation.width  / (MAX_CRAFTING_SLOTS + 1);
			float slotH = craftingStation.height / (MAX_CRAFTING_SLOTS + 1);

			float gapX =
				(craftingStation.width - MAX_CRAFTING_SLOTS * slotW)
				/ (MAX_CRAFTING_SLOTS + 1);

			float gapY =
				(craftingStation.height - MAX_CRAFTING_SLOTS * slotH)
				/ (MAX_CRAFTING_SLOTS + 1);

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

	// drawSimpleQuad(hotbar.x, hotbar.y, hotbar.width, hotbar.height, glm::vec4(0,0,0,0.5));
	uint8_t i = 0;
	for (auto &hotbarSlotCoord : hotbarSlots)
	{
		drawSimpleQuad(hotbarSlotCoord.x, hotbarSlotCoord.y, hotbarSlotCoord.width, hotbarSlotCoord.height, hotbarColor);
		if (i == activeHotbarSlot)
			drawSimpleQuad(hotbarSlotCoord.x, hotbarSlotCoord.y, hotbarSlotCoord.width, hotbarSlotCoord.height, glm::vec4(0,0,0,0.4f));

		i++;
	}

	i = 0;
	for (auto &hotbarSlotCoord : hotbarSlots)
	{
		textRenderer.renderText(std::to_string(getSlot(i).second), hotbarSlotCoord.x, hotbarSlotCoord.y + hotbar.height * 0.7, glm::vec3(1.0f));

		//could optimize and only redo if inventory/hotbar has changed. TODO ?
		std::visit([&](const auto& value) {
			using T = std::decay_t<decltype(value)>;
			if constexpr (std::is_same_v<T, BlockType>) {
				if (value != BlockType::BEGIN)
					build2DInventoryCube(meshVertices, glm::vec2(hotbarSlotCoord.x + 18 * menuScale, hotbarSlotCoord.y + 5 * menuScale), 40 * menuScale, value, textureManager);
			} else if constexpr (std::is_same_v<T, WeaponType>) {
				// handle WeaponType
			} else {
				// handle MiscType
			}
		}, getItemAtSlot(i));
		i++;
	}

	setupCubes(meshVertices);

	glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

int InventoryUI::getSlotAt(double mouseX, double mouseY) const
{
	mouseY = fullscreenHeight - mouseY;
    for (int i = 0; i < rows * cols; ++i)
    {
        const auto& s = inventorySlots[i];
        if (mouseX >= s.x && mouseX <= s.x + s.width &&
            mouseY >= s.y && mouseY <= s.y + s.height)
            return i;
    }
    return -1;
}

void InventoryUI::handleMouseClick(double mouseX, double mouseY, int button, int action)
{
	if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
	{
		int slot = getSlotAt(mouseX, mouseY);
		if (slot != -1)
		{
			this->mouseX = mouseX;
			this->mouseY = mouseY;
			lastAction = {slot, InventoryActionType::INV_LEFT_CLICK};
		}
	}
	else if (button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
	{
		int slot = getSlotAt(mouseX, mouseY);
		if (slot != -1)
		{
			this->mouseX = mouseX;
			this->mouseY = mouseY;
			lastAction = {slot, InventoryActionType::INV_RIGHT_CLICK};
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
	for (int i = 0; i < rows * cols; i++)
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
	for (int i = 0; i < MAX_CRAFTING_SLOTS * MAX_CRAFTING_SLOTS; i++)
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

	for (int i = 0; i < rows * cols; i++)
	{
		textRenderer.renderText(std::to_string(getSlot(i).second), inventorySlots[i].x, inventorySlots[i].y + hotbar.height * 0.7, glm::vec3(1.0f));
		
		//could optimize and only redo if inventory/hotbar has changed. TODO ?
		std::visit([&](const auto& value) {
			using T = std::decay_t<decltype(value)>;
				if constexpr (std::is_same_v<T, BlockType>) {
					if (value != BlockType::BEGIN)
						build2DInventoryCube(meshVertices, glm::vec2(inventorySlots[i].x + 18 * menuScale, inventorySlots[i].y + 5 * menuScale), 40 * menuScale, value, textureManager);
				} else if constexpr (std::is_same_v<T, WeaponType>) {
					// handle WeaponType
				} else {
					// handle MiscType
				}
			}, getItemAtSlot(i));
	}

	if (getHand().second != 0)
	{
		textRenderer.renderText(std::to_string(getHand().second), mouseX, fullscreenHeight - mouseY, glm::vec3(1.0f));

		std::visit([&](const auto& value) {
			using T = std::decay_t<decltype(value)>;
			if constexpr (std::is_same_v<T, BlockType>) {
				if (value != BlockType::BEGIN)
					build2DInventoryCube(meshVertices, glm::vec2(mouseX, fullscreenHeight - mouseY), 40 * menuScale, value, textureManager);
			} else if constexpr (std::is_same_v<T, WeaponType>) {
				// handle WeaponType
			} else {
				// handle MiscType
			}
		}, getHand().first);
	}

	setupCubes(meshVertices);
}