#ifndef INVENTORY_UI
#define INVENNTORY_UI

#include "Inventory.hpp"
#include "Menu.hpp"
#include "Typer.hpp"
#include "blockRenderingHelperFunctions.hpp"

int constexpr MAX_SLOTS = 9;

class InventoryUI : public Inventory, public Menu
{
	int MAX_BUFFER_SIZE = sizeof(float) * rows * cols * (2 + 2) * 3 * 6; //2 coords, 2 uvs. 3 faces, 6 vertices
	std::unique_ptr<Shader> shader;
	uint texture;

	Typer textRenderer;

	struct hotbarSlotCoords
	{
		int hotbarSlotX;
		int hotbarSlotY;
		int hotbarSlotW;
		int hotbarSlotH;
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

		InventoryUI(float width, float height);
		~InventoryUI() = default;

		void drawHotbar();
		void drawInventory() const;
		void onRender() override {};
};

#endif