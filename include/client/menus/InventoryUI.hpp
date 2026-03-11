#ifndef INVENTORY_UI
#define INVENTORY_UI

#include "Inventory.hpp"
#include "Menu.hpp"
#include "blockRenderingHelperFunctions.hpp"
#include "TextureManager.hpp"
#include "Network.hpp"
#include <optional>
#include <filesystem>

#include "VideoPlayer.hpp"

int constexpr MAX_SLOTS = 9;
int constexpr MAX_CRAFTING_SLOTS = 3; //3x3 but whatever

class InventoryUI : public Inventory, public Menu
{
	int MAX_BUFFER_SIZE = sizeof(float) * (rows * cols + 1) * (2 + 2 + 1) * 3 * 6; //2 coords, 2 uvs, 1 texLayer. 3 faces, 6 vertices
	std::unique_ptr<Shader> shader;
	const TextureManager* textureManager = nullptr;
	uint texture;
	uint badAppleTex;
	// std::unique_ptr<VideoPlayer> videoPlayer;

	// coords of every slot in hotbar
	struct HotbarSlotCoords
	{
		float x;
		float y;
		float width;
		float height;
	};
	HotbarSlotCoords hotbarSlots[MAX_SLOTS];

	// coords of the hotbar
	struct HotbarCoords
	{
		float x;
		float y;
		float width;
		float height;
	};
	HotbarCoords hotbar;

	// coords of the inventory layout
	struct InventoryLayoutCoords
	{
		float x;
		float y;
		float width;
		float height;
	};
	InventoryLayoutCoords inventoryLayout;

	struct InventoryCoords
	{
		float x;
		float y;
		float width;
		float height;
	};
	InventoryCoords inventory;

	struct InventorySlots
	{
		float x;
		float y;
		float width;
		float height;
	};
	InventorySlots inventorySlots[rows * cols];

	struct BlackApple
	{
		float x;
		float y;
		float width;
		float height;
	};
	BlackApple blackApple;

	struct CraftingStation
	{
		float x;
		float y;
		float width;
		float height;
	};
	CraftingStation craftingStation;

	struct CraftingStationSlots
	{
		float x;
		float y;
		float width;
		float height;
	};
	CraftingStation craftingStationSlots[MAX_CRAFTING_SLOTS * MAX_CRAFTING_SLOTS];

	struct CraftingResultSlot
	{
		float x;
		float y;
		float width;
		float height;
	};
	CraftingResultSlot craftingResultSlot;

	glm::vec4 hotbarColor;

	GLuint inventoryTextureVAO = 0;
	GLuint inventoryTextureVBO = 0;

	void initGL();
	void setupCubes(const std::vector<float> &meshVertices);

	void onRender() override;
	void build() override;
	void drawEveryInventoryQuad();

	int getSlotAt(double mouseX, double mouseY) const;
	void handleMouseClick(double mouseX, double mouseY, int button, int action) override;
	void handleMouseMove(double mouseX, double mouseY) override;

	double mouseX = 0;
	double mouseY = 0;

	public:

		InventoryUI(int width, int height, const TextureManager* texMgr = nullptr);
		~InventoryUI();

		void drawHotbar();
		void drawInventory() const;

		std::optional<std::pair<int, InventoryActionType>> lastAction; //awful solution
};

#endif