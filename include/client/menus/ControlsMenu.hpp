
#ifndef CONTROLS_MENU_HPP
#define CONTROLS_MENU_HPP

#include "Menu.hpp"
#include <array>

#define CONTROL_LIST 		\
    X(FORWARD)       		\
    X(BACKWARD)      		\
    X(LEFT)          		\
    X(RIGHT)         		\
    X(UP)            		\
    X(DOWN)             	\
    X(MOVE_FAST)        	\
    X(TOGGLE_FULLSCREEN)	\
    X(TOGGLE_WIREFRAME)		\
    X(TOGGLE_DEBUG)			\
	X(THIRD_PERSON_CAMERA)	\
	X(PLAYER_LIST)			\
							\
    X(DESTROY_BLOCK)       	\
	X(PLACE_BLOCK)      	\
    X(TOGGLE_SHADER)		\
    X(CLOSE_WINDOW)			\
	X(HOTBAR_1)				\
	X(HOTBAR_2)				\
	X(HOTBAR_3)				\
	X(HOTBAR_4)				\
	X(HOTBAR_5)				\
	X(HOTBAR_6)				\
	X(HOTBAR_7)				\
	X(HOTBAR_8)				\
	X(HOTBAR_9)				\

enum controls {
#define X(name) name,
    CONTROL_LIST
#undef X
    CONTROL_COUNT
};

class ControlsMenu : public Menu {
	public:
		ControlsMenu(float width, float height, GLuint dirtTex);
		~ControlsMenu();

		std::array<int, CONTROL_COUNT> getControlsArray() const { return controlsArray; }

		bool changeControl(int key);

	private:
		std::array<int, CONTROL_COUNT> controlsArray;
		std::array<Button, CONTROL_COUNT> controlsButtons;
		bool changeRequested = false;
		int controlToChange = 0;

		//keeps track of control names so they can be inserted/read from the .config file
		const char* controlNames[CONTROL_COUNT] = {
		#define X(name) #name,
			CONTROL_LIST
		#undef X
		};

		GLuint dirtTexture = 0;

		void onRender() override;
		void build() override;

		void handleMouseClick(double mouseX, double mouseY, int button, int action) override;
		void handleMouseMove(double mouseX, double mouseY) override;

		void loadControlsDefaults();
		void saveControls(const char* filename = "controls.cfg");
		void loadControlsFromFile(const char* filename = "controls.cfg");
};	

#endif