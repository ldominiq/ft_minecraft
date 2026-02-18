#include "InventoryUI.hpp"

constexpr float textScale = 0.3f;

InventoryUI::InventoryUI(float width, float height): Inventory(), Menu(width, height), textRenderer("fonts/Roboto-Regular.ttf", textScale)
{
	shader = std::make_unique<Shader>("shaders/InventoryCube.vert", "shaders/InventoryCube.frag"); //should probably reuse cubePropShader.frag
	texture = shader->loadTexture("assets/textures/textures.png");

	initGL();

	textRenderer.setProjection(width, height);

	hotbarX = width / 5.0f;
	hotbarY = height / 100.0f;;
	hotbarW = (width / 5.0f) * 3;
	hotbarH = height / 10.0f;

	for (int i = 0; i < MAX_SLOTS ; i++)
	{
		hotbarSlots[i].width = hotbarW  / (MAX_SLOTS + 1);
		hotbarSlots[i].height = hotbarH - hotbarH / 10.f;

		hotbarSlots[i].x = hotbarX + ((hotbarW  / (MAX_SLOTS)) / MAX_SLOTS) + ((hotbarSlots[i].width / MAX_SLOTS) + hotbarSlots[i].width) * i;
		hotbarSlots[i].y = hotbarY + (hotbarH  - hotbarSlots[i].height) / 2.0f;
	}
	// textRenderer.renderText("12345", width/2.0f, height/2.0f, glm::vec3(1.0f));

	hotbarColor = glm::vec4(0.0f,0.0f,0.0f,0.5f);
}

InventoryUI::~InventoryUI()
{
	if (glfwGetCurrentContext()) {
		glDeleteTextures(1, &texture);
		glDeleteVertexArrays(1, &inventoryTextureVAO);
		glDeleteBuffers(1, &inventoryTextureVBO);
	} else {
		inventoryTextureVAO = 0;
		inventoryTextureVBO = 0;
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
		4 * sizeof(float),
		(void*)0
	);
	glEnableVertexAttribArray(0);

	// uv
	glVertexAttribPointer(
		1, 2, GL_FLOAT, GL_FALSE,
		4 * sizeof(float),
		(void*)(2 * sizeof(float))
	);
	glEnableVertexAttribArray(1);

	glBindVertexArray(0);
}

void InventoryUI::setupCubes(const std::vector<float> &meshVertices)
{
	shader->use();

	glBindVertexArray(inventoryTextureVAO);

	shader->setInt("atlas", 0);
	shader->setVec2("uScreenSize", glm::vec2(width, height));

	// texture
	glActiveTexture(GL_TEXTURE0);
	glBindTexture(GL_TEXTURE_2D, texture);

	glBindBuffer(GL_ARRAY_BUFFER, inventoryTextureVBO);
	glBufferSubData(
		GL_ARRAY_BUFFER,
		0,
		meshVertices.size() * sizeof(float),
		meshVertices.data()
	);

	// draw
	glDrawArrays(GL_TRIANGLES, 0, meshVertices.size() / 4);
}

void InventoryUI::drawHotbar()
{
	glDisable(GL_DEPTH_TEST);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	std::vector<float> meshVertices;

	// drawSimpleQuad(hotbarX, hotbarY, hotbarW, hotbarH, glm::vec4(0,0,0,0));
	uint8_t i = 0;
	for (auto &hotbarSlotCoord : hotbarSlots)
	{
		drawSimpleQuad(hotbarSlotCoord.x, hotbarSlotCoord.y, hotbarSlotCoord.width, hotbarSlotCoord.height, hotbarColor);
		textRenderer.renderText(std::to_string(getSlot(i).second), hotbarSlotCoord.x, hotbarSlotCoord.y + hotbarH * (1 - textScale), glm::vec3(1.0f));

		//could optimize and only redo if inventory/hotbar has changed. TODO ?
		std::visit([&](const auto& value) {
			using T = std::decay_t<decltype(value)>;
			if constexpr (std::is_same_v<T, BlockType>) {
				if (value != BlockType::BEGIN)
					build2DInventoryCube(meshVertices, glm::vec2(hotbarSlotCoord.x + 18, hotbarSlotCoord.y + 5), 40, value);
			} else if constexpr (std::is_same_v<T, WeaponType>) {
				// handle WeaponType
			} else {
				// handle MiscType
			}
		}, getItemAtSlot(i));

		if (i == activeHotbarSlot)
			drawSimpleQuad(hotbarSlotCoord.x, hotbarSlotCoord.y, hotbarSlotCoord.width, hotbarSlotCoord.height, glm::vec4(0,0,0,0.4f));

		i++;
	}

	setupCubes(meshVertices);

	glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}
