#include "ControlsMenu.hpp"

static constexpr float DESIGN_W = 960.0f;
static constexpr float DESIGN_H = 540.0f;

static constexpr float BTN_W = 200.0f;
static constexpr float BTN_H = 40.0f;

ControlsMenu::ControlsMenu(float width, float height, GLuint dirtTex)
	: Menu(DESIGN_W, DESIGN_H), dirtTexture(dirtTex)
{
	resize(width, height);
	loadControlsFromFile();
	build();
}

ControlsMenu::~ControlsMenu()
{
	saveControls();
}

std::string getKeyName(int key)
{
    int scancode = glfwGetKeyScancode(key);
    const char* name = glfwGetKeyName(key, scancode);

    if (name)
        return std::string(name);

    switch (key)
    {
        case GLFW_KEY_SPACE: return "Space";
        case GLFW_KEY_ENTER: return "Enter";
        case GLFW_KEY_ESCAPE: return "Escape";
        case GLFW_KEY_LEFT_SHIFT: return "Left Shift";
        case GLFW_KEY_RIGHT_SHIFT: return "Right Shift";
        case GLFW_KEY_LEFT_CONTROL: return "Left Ctrl";
        case GLFW_KEY_RIGHT_CONTROL: return "Right Ctrl";
        case GLFW_KEY_TAB: return "Tab";
        case GLFW_KEY_F1: return "F1";
        case GLFW_KEY_F2: return "F2";
        case GLFW_KEY_F5: return "F5";
        case GLFW_KEY_F6: return "F6";
        case GLFW_KEY_F11: return "F11";
		case GLFW_MOUSE_BUTTON_LEFT: return "Left Click";
		case GLFW_MOUSE_BUTTON_RIGHT: return "Right Click";
		case GLFW_KEY_RIGHT: return "Right Arrow";
		case GLFW_KEY_LEFT: return "Left Arrow";
		case GLFW_KEY_UP: return "Up Arrow";
		case GLFW_KEY_DOWN: return "Down Arrow";
        default: return "Unknown";
    }
}

void ControlsMenu::build()
{
	float centerX = fullscreenWidth / 2.0f;
	float fourthX = fullscreenWidth / 4.0f;
	float twelfthX = fullscreenWidth / 12.0f;
	float quarterY = fullscreenHeight / 3.0f;

	float btnW = BTN_W * menuScale;
	float btnH = BTN_H * menuScale;

	for (int i = 0; i < CONTROL_COUNT && i < AMOUNT_OF_CONFIGURABLE_CONTROLS; i++)
	{
		if (i % 3 == 0)
			controlsButtons[i].x = fourthX - btnW / 2.0f - twelfthX;
		else if (i % 3 == 1)
			controlsButtons[i].x = fourthX * 2 - btnW / 2.0f;
		else
			controlsButtons[i].x = fourthX * 3 - btnW / 2.0f + twelfthX;

		controlsButtons[i].w = btnW;
		controlsButtons[i].h = btnH;
		controlsButtons[i].y = fullscreenHeight - quarterY - (i/3 + 1) * (controlsButtons[i].h + 25);
		controlsButtons[i].label = std::string(controlNames[i]) + ": " + (controlsArray[i] >= 0 ? getKeyName(controlsArray[i]) : "Unbound");
	}

	saveButton.w = btnW;
	saveButton.h = btnH;
	saveButton.x = centerX - btnW / 2.0f;
	saveButton.y = 60.0f * menuScale;
}

void ControlsMenu::onRender()
{
	drawTiledBackground(dirtTexture);

	// Title (high-resolution Typer for crisp rendering)
	float savedScale = textRenderer.getScale();
	titleRenderer.setScale(0.45f * menuScale);
	titleRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	std::string title = "Controls";
	float titleWidth = titleRenderer.getPixelSizeOfString(title);
	float titleX = (fullscreenWidth - titleWidth) / 2.0f;
	float titleY = fullscreenHeight - 100.0f * menuScale;

	titleRenderer.renderText(title, titleX, titleY, glm::vec3(1.0f));

	textRenderer.setScale(savedScale);

	for (auto &button : controlsButtons)
	{
		int i = &button - controlsButtons.data();
		std::string label = button.label;
		if (changeRequested && i == controlToChange)
		{
			label = std::string(controlNames[i]) + ": ";
			bool showCursor = static_cast<int>(glfwGetTime() * 2.0f) % 2 == 0;
			if (showCursor) label += ". . .";
		}
		drawButton(button.x, button.y, button.w, button.h, label, button.hovered);
	}

	drawButton(saveButton.x, saveButton.y, saveButton.w, saveButton.h, "Save", saveButton.hovered);
}

bool ControlsMenu::changeControl(int key)
{
	if (!changeRequested) return false;

	controlsArray[controlToChange] = key;
	controlsButtons[controlToChange].label = std::string(controlNames[controlToChange]) + ": " + getKeyName(controlsArray[controlToChange]);
	changeRequested = false;

	return true;
}

void ControlsMenu::handleMouseMove(double mouseX, double mouseY)
{
	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	for (auto &button : controlsButtons) {
		button.hovered =
			glX >= button.x && glX <= button.x + button.w &&
			glY >= button.y && glY <= button.y + button.h;
	}

	saveButton.hovered =
		glX >= saveButton.x && glX <= saveButton.x + saveButton.w &&
		glY >= saveButton.y && glY <= saveButton.y + saveButton.h;
}

void ControlsMenu::handleMouseClick(double mouseX, double mouseY, int button, int action)
{
	if (button != GLFW_MOUSE_BUTTON_LEFT || action != GLFW_PRESS)
		return;

	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	for (auto &button : controlsButtons) {
		if (glX >= button.x && glX <= button.x + button.w &&
			glY >= button.y && glY <= button.y + button.h) {
			changeRequested = true;
			controlToChange = &button - controlsButtons.data(); // index of the button in the array
		}
	}

	if (glX >= saveButton.x && glX <= saveButton.x + saveButton.w &&
		glY >= saveButton.y && glY <= saveButton.y + saveButton.h) {
		if (onSave) onSave();
	}
}

void ControlsMenu::loadControlsDefaults() {
	controlsArray[FORWARD]				= GLFW_KEY_W;
	controlsArray[BACKWARD]        		= GLFW_KEY_S;
	controlsArray[LEFT]					= GLFW_KEY_A;
	controlsArray[RIGHT]				= GLFW_KEY_D;
    controlsArray[UP]					= GLFW_KEY_SPACE;
    controlsArray[DOWN]					= GLFW_KEY_LEFT_SHIFT;
    controlsArray[DESTROY_BLOCK]		= GLFW_MOUSE_BUTTON_LEFT;
    controlsArray[PLACE_BLOCK]			= GLFW_MOUSE_BUTTON_RIGHT;
    controlsArray[TOGGLE_FULLSCREEN]	= GLFW_KEY_F11;
    controlsArray[TOGGLE_WIREFRAME]		= GLFW_KEY_F1;
    controlsArray[TOGGLE_SHADER]		= GLFW_KEY_F2;
    controlsArray[TOGGLE_DEBUG]			= GLFW_KEY_F6;
    controlsArray[MOVE_FAST]			= GLFW_KEY_LEFT_CONTROL;
    controlsArray[CLOSE_WINDOW]			= GLFW_KEY_ESCAPE;
	controlsArray[THIRD_PERSON_CAMERA]	= GLFW_KEY_F5;
	controlsArray[PLAYER_LIST]			= GLFW_KEY_TAB;
	controlsArray[TOGGLE_INVENTORY]		= GLFW_KEY_E;
	
	controlsArray[HOTBAR_1]				= GLFW_KEY_1;
	controlsArray[HOTBAR_2]				= GLFW_KEY_2;
	controlsArray[HOTBAR_3]				= GLFW_KEY_3;
	controlsArray[HOTBAR_4]				= GLFW_KEY_4;
	controlsArray[HOTBAR_5]				= GLFW_KEY_5;
	controlsArray[HOTBAR_6]				= GLFW_KEY_6;
	controlsArray[HOTBAR_7]				= GLFW_KEY_7;
	controlsArray[HOTBAR_8]				= GLFW_KEY_8;
	controlsArray[HOTBAR_9]				= GLFW_KEY_9;
}

void ControlsMenu::loadControlsFromFile(const char* filename) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        loadControlsDefaults();
        return;
    }

    // Initialize defaults first
    loadControlsDefaults();

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string keyName;
        int keyValue;
        if (!(iss >> keyName >> keyValue)) continue;

        for (int i = 0; i < CONTROL_COUNT; ++i) {
            if (keyName == controlNames[i]) {
                controlsArray[i] = keyValue;
                break;
            }
        }
    }
}

void ControlsMenu::saveControls(const char* filename) {
    std::ofstream file(filename);
    if (!file.is_open()) return; // handle errors as you want

    for (int i = 0; i < CONTROL_COUNT; ++i) {
        file << controlNames[i] << " " << controlsArray[i] << "\n";
    }

	file << "\n\n# see 'https://www.glfw.org/docs/latest/group__keys.html' for key values" << '\n';
}
