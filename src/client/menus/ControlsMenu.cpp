#include "ControlsMenu.hpp"

#include <cctype>

static constexpr float DESIGN_W = 960.0f;
static constexpr float DESIGN_H = 540.0f;

// Layout (design units)
static constexpr float ROW_W       = 420.0f; // total row width (label area + key button)
static constexpr float ROW_H       = 30.0f;
static constexpr float ROW_GAP_Y   = 8.0f;
static constexpr float COL_GAP_X   = 30.0f;
static constexpr float KEY_BTN_W   = 140.0f; // key bind button width
static constexpr float LABEL_PAD_X = 12.0f;  // padding between label text and key button

static constexpr float SAVE_BTN_W  = 200.0f;
static constexpr float SAVE_BTN_H  = 40.0f;

static constexpr int CONFIGURABLE_COLUMNS = 2;

static std::string prettyControlName(const char* raw)
{
    std::string out;
    bool capitalize = true;
    for (const char* p = raw; *p; ++p) {
        if (*p == '_') {
            out += ' ';
            capitalize = true;
        } else if (capitalize) {
            out += static_cast<char>(std::toupper(static_cast<unsigned char>(*p)));
            capitalize = false;
        } else {
            out += static_cast<char>(std::tolower(static_cast<unsigned char>(*p)));
        }
    }
    return out;
}

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
	}

	int scancode = glfwGetKeyScancode(key);

	if (scancode > 0) {

		if (const char* name = glfwGetKeyName(key, scancode))
			return std::string(name);
	}

	return "Unknown";
}

void ControlsMenu::build()
{
	float centerX = fullscreenWidth / 2.0f;

	float rowW   = ROW_W     * menuScale;
	float rowH   = ROW_H     * menuScale;
	float rowGap = ROW_GAP_Y * menuScale;
	float colGap = COL_GAP_X * menuScale;
	float keyW   = KEY_BTN_W * menuScale;

	const int rowsPerCol = (AMOUNT_OF_CONFIGURABLE_CONTROLS + CONFIGURABLE_COLUMNS - 1) / CONFIGURABLE_COLUMNS;

	float gridW = CONFIGURABLE_COLUMNS * rowW + (CONFIGURABLE_COLUMNS - 1) * colGap;
	float gridLeft = centerX - gridW / 2.0f;

	// Vertical placement: centered between title and save button
	float titleBottom    = fullscreenHeight - 140.0f * menuScale;
	float saveTop        = (60.0f + SAVE_BTN_H) * menuScale + 20.0f * menuScale;
	float available      = titleBottom - saveTop;
	float gridH          = rowsPerCol * rowH + (rowsPerCol - 1) * rowGap;
	float gridTop        = saveTop + (available + gridH) / 2.0f; // top edge of first row
	if (gridTop > titleBottom) gridTop = titleBottom;

	for (int i = 0; i < CONTROL_COUNT; i++)
	{
		if (i < AMOUNT_OF_CONFIGURABLE_CONTROLS) {
			int col = i / rowsPerCol;
			int row = i % rowsPerCol;

			float rowX = gridLeft + col * (rowW + colGap);
			float rowY = gridTop - rowH - row * (rowH + rowGap);

			// Button is the right-hand portion of the row (only the key button is clickable)
			controlsButtons[i].x = rowX + rowW - keyW;
			controlsButtons[i].y = rowY;
			controlsButtons[i].w = keyW;
			controlsButtons[i].h = rowH;
			controlsButtons[i].label = (controlsArray[i] >= 0) ? getKeyName(controlsArray[i]) : "Unbound";
		} else {
			// Non-configurable entries get zero-sized buttons (never rendered or clicked)
			controlsButtons[i].x = 0;
			controlsButtons[i].y = 0;
			controlsButtons[i].w = 0;
			controlsButtons[i].h = 0;
			controlsButtons[i].label.clear();
		}
	}

	float saveBtnW = SAVE_BTN_W * menuScale;
	float saveBtnH = SAVE_BTN_H * menuScale;
	saveButton.w = saveBtnW;
	saveButton.h = saveBtnH;
	saveButton.x = centerX - saveBtnW / 2.0f;
	saveButton.y = 60.0f * menuScale;
}

void ControlsMenu::onRender()
{
	drawTiledBackground(dirtTexture);

	float savedScale = textRenderer.getScale();

	// Title (high-resolution Typer for crisp rendering)
	titleRenderer.setScale(0.45f * menuScale);
	titleRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	std::string title = "Controls";
	float titleWidth = titleRenderer.getPixelSizeOfString(title);
	float titleX = (fullscreenWidth - titleWidth) / 2.0f;
	float titleY = fullscreenHeight - 100.0f * menuScale;

	titleRenderer.renderText(title, titleX, titleY, glm::vec3(1.0f));

	// Action labels (left of each row) + key buttons (right of each row)
	const float labelAreaW = (ROW_W - KEY_BTN_W - LABEL_PAD_X) * menuScale;
	const float baseLabelScale = 0.35f * menuScale;
	const glm::vec3 labelColor(0.95f);
	const glm::vec3 promptColor(1.0f, 0.85f, 0.3f);

	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);

	for (int i = 0; i < AMOUNT_OF_CONFIGURABLE_CONTROLS && i < CONTROL_COUNT; i++)
	{
		Button &button = controlsButtons[i];

		// Action name drawn as plain text on the left side of the row
		std::string actionText = prettyControlName(controlNames[i]);
		float useScale = baseLabelScale;
		textRenderer.setScale(useScale);
		float textW = static_cast<float>(textRenderer.getPixelSizeOfString(actionText));
		if (textW > labelAreaW && textW > 0.0f) {
			useScale = baseLabelScale * (labelAreaW / textW);
			textRenderer.setScale(useScale);
			textW = static_cast<float>(textRenderer.getPixelSizeOfString(actionText));
		}
		float ascent = textRenderer.getAscent();
		float rowLeftX = button.x - (ROW_W - KEY_BTN_W) * menuScale;
		float labelX = rowLeftX;
		float labelY = button.y + (button.h - ascent) / 2.0f;
		textRenderer.renderText(actionText, labelX, labelY, labelColor);

		// Key bind button on the right
		std::string keyLabel;
		bool waiting = (changeRequested && i == controlToChange);
		if (waiting) {
			bool showCursor = static_cast<int>(glfwGetTime() * 2.0f) % 2 == 0;
			keyLabel = showCursor ? "> . . . <" : "> <";
		} else {
			keyLabel = button.label.empty() ? std::string("Unbound") : button.label;
		}

		// Auto-shrink key label if it overflows the button
		float keyScale = 0.5f * menuScale;
		textRenderer.setScale(keyScale);
		float keyTextW = static_cast<float>(textRenderer.getPixelSizeOfString(keyLabel));
		float keyMaxW = button.w - 16.0f * menuScale;
		if (keyTextW > keyMaxW && keyTextW > 0.0f) {
			keyScale = keyScale * (keyMaxW / keyTextW);
		}

		// drawButton uses its own internal text scale (0.5 * menuScale); for long key
		// names we draw the background manually and overlay the label at our shrunken scale.
		if (keyScale < 0.5f * menuScale - 0.001f) {
			// Mimic drawButton background
			glm::vec4 bgColor = button.hovered
				? glm::vec4(0.4f, 0.4f, 0.5f, 0.9f)
				: glm::vec4(0.25f, 0.25f, 0.3f, 0.85f);
			float border = 2.0f * menuScale;
			glm::vec4 borderColor = button.hovered
				? glm::vec4(0.7f, 0.7f, 0.8f, 1.0f)
				: glm::vec4(0.4f, 0.4f, 0.4f, 1.0f);
			drawSimpleQuad(button.x - border, button.y - border,
						   button.w + 2 * border, button.h + 2 * border, borderColor);
			drawSimpleQuad(button.x, button.y, button.w, button.h, bgColor);

			textRenderer.setScale(keyScale);
			float kAscent = textRenderer.getAscent();
			float kw = static_cast<float>(textRenderer.getPixelSizeOfString(keyLabel));
			float kx = button.x + (button.w - kw) / 2.0f;
			float ky = button.y + (button.h - kAscent) / 2.0f;
			textRenderer.renderText(keyLabel, kx, ky, waiting ? promptColor : glm::vec3(1.0f));
		} else {
			drawButton(button.x, button.y, button.w, button.h, keyLabel, button.hovered);
		}
	}

	textRenderer.setScale(savedScale);

	drawButton(saveButton.x, saveButton.y, saveButton.w, saveButton.h, "Save", saveButton.hovered);
}

bool ControlsMenu::changeControl(int key)
{
	if (!changeRequested) return false;

	controlsArray[controlToChange] = key;
	controlsButtons[controlToChange].label = getKeyName(controlsArray[controlToChange]);
	changeRequested = false;

	return true;
}

void ControlsMenu::handleMouseMove(double mouseX, double mouseY)
{
	float glY = fullscreenHeight - static_cast<float>(mouseY);
	float glX = static_cast<float>(mouseX);

	for (int i = 0; i < AMOUNT_OF_CONFIGURABLE_CONTROLS && i < CONTROL_COUNT; i++) {
		Button &button = controlsButtons[i];
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

	for (int i = 0; i < AMOUNT_OF_CONFIGURABLE_CONTROLS && i < CONTROL_COUNT; i++) {
		Button &b = controlsButtons[i];
		if (glX >= b.x && glX <= b.x + b.w &&
			glY >= b.y && glY <= b.y + b.h) {
			changeRequested = true;
			controlToChange = i;
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
    controlsArray[SNEAK]				= GLFW_KEY_LEFT_SHIFT;
    controlsArray[DESTROY_BLOCK]		= GLFW_MOUSE_BUTTON_LEFT;
    controlsArray[PLACE_BLOCK]			= GLFW_MOUSE_BUTTON_RIGHT;
    controlsArray[TOGGLE_FULLSCREEN]	= GLFW_KEY_F11;
    controlsArray[TOGGLE_WIREFRAME]		= GLFW_KEY_F1;
    controlsArray[TOGGLE_HUD]			= GLFW_KEY_F2;
    controlsArray[TOGGLE_DEBUG]			= GLFW_KEY_F6;
    controlsArray[RUN]					= GLFW_KEY_LEFT_CONTROL;
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
