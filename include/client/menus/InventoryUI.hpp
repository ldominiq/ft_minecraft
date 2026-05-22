#ifndef INVENTORY_UI
#define INVENTORY_UI

#include "PlayerInventory.hpp"
#include "CraftingStation.hpp"

#include "Menu.hpp"
#include "blockRenderingHelperFunctions.hpp"
#include "TextureManager.hpp"
#include "Network.hpp"
#include <optional>
#include <filesystem>
#include "Protocol.hpp"

class InventoryUI : public Menu
{
	int MAX_BUFFER_SIZE = 0;
	std::unique_ptr<Shader> shader;
	const TextureManager* textureManager = nullptr;
	uint texture;
	uint badAppleTex;
	// std::unique_ptr<VideoPlayer> videoPlayer;

	std::weak_ptr<PlayerInventory> playerInventory;
	std::weak_ptr<CraftingStation> craftingStationInv;
	std::weak_ptr<std::pair<ItemType, itemStackSize_t>> handPtr;

	bool dragging = false;
	int dragButton = -1;
	int lastHoveredSlot = -1;

	int inventoryRows = 0;
	int inventoryCols = 0;
	int craftingStationRows = 0;
	int craftingStationCols = 0;

	// coords of every slot in hotbar
	struct HotbarSlotCoords
	{
		float x;
		float y;
		float width;
		float height;
	};
	std::vector<HotbarSlotCoords> hotbarSlots; // size is PlayerInventory rows

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
	std::vector<InventorySlots> inventorySlots; // size is PlayerInventory rows * cols

	struct BlackApple
	{
		float x;
		float y;
		float width;
		float height;
	};
	BlackApple blackApple;

	struct CraftingStationSize
	{
		float x;
		float y;
		float width;
		float height;
	};
	CraftingStationSize craftingStation;

	struct CraftingStationSlots
	{
		float x;
		float y;
		float width;
		float height;
	};
	std::vector<CraftingStationSlots> craftingStationSlots; // size is CraftingStation rows * cols

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

	InventoryType getCurrentInventoryType(double mouseX, double mouseY) const;
	int getCraftingSlotAt(double mouseX, double mouseY) const;
	int getSlotAt(double mouseX, double mouseY) const;
	void handleMouseClick(double mouseX, double mouseY, int button, int action) override;
	void handleMouseMove(double mouseX, double mouseY) override;
	void handleInventoryModifiers(NetInventoryAction &pkt, int action, int button);

	double mouseX = 0;
	double mouseY = 0;

	// Death screen bookkeeping. -1 means "not currently dead"; otherwise stores
	// the glfwGetTime() value at which the death animation began so the fade-in
	// is wall-clock based rather than frame-count based.
	double deathStartTime = -1.0;

	public:

		InventoryUI(int width,
					int height,
					const TextureManager* texMgr = nullptr,
					std::shared_ptr<PlayerInventory> playerInv = nullptr,
					std::shared_ptr<CraftingStation> craftingStation = nullptr,
					std::shared_ptr<InventoryExternalVariablesRefs> inventoryExternalVarsRefs = nullptr);
		~InventoryUI();

		//return value corresponds to where we dragged over a new slot or not.
		bool checkInventoryDrag(NetInventoryAction &pkt);
		void drawHotbar();
		void drawHealth(float health) const;
		void drawCrosshair() const;
		// Red vignette + "YOU DIED" overlay shown while local health<=0; fades in
		// from the moment of death and resets once we respawn. Cheap: a few quads
		// plus two text draws. Safe to call every frame regardless of state.
		void drawDeathScreen(float health);
		void drawInventory() const;

		std::optional<NetInventoryAction> lastAction; //awful solution
};

#endif