#ifndef INVENTORY_UI
#define INVENTORY_UI

#include "Inventory.hpp"
#include "Menu.hpp"
#include "Typer.hpp"
#include "blockRenderingHelperFunctions.hpp"
#include "TextureManager.hpp"

int constexpr MAX_SLOTS = 9;

class InventoryUI : public Inventory, public Menu
{
	int MAX_BUFFER_SIZE = sizeof(float) * rows * cols * (2 + 2 + 1) * 3 * 6; //2 coords, 2 uvs, 1 texLayer. 3 faces, 6 vertices
	std::unique_ptr<Shader> shader;
	const TextureManager* textureManager = nullptr;

	Typer textRenderer;

	struct hotbarSlotCoords
	{
		int x;
		int y;
		int width;
		int height;
	};
	hotbarSlotCoords hotbarSlots[MAX_SLOTS];

	int hotbarX;
	int hotbarY;
	int hotbarW;
	int hotbarH;
	glm::vec4 hotbarColor;

	GLuint inventoryTextureVAO = 0;
	GLuint inventoryTextureVBO = 0;

	void initGL();
	void setupCubes(const std::vector<float> &meshVertices);

	public:

		InventoryUI(float width, float height, const TextureManager* texMgr = nullptr);
		~InventoryUI();

		void drawHotbar();
		void drawInventory() const;
		void onRender() override {};
};

#endif