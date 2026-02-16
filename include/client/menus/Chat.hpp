
#ifndef CHAT_HPP
#define CHAT_HPP

#include "Typer.hpp"
#include "Menu.hpp"

//menus.. and everything really should use an EBO (indexes) to spare vertices... but it's kinda whatever at this point.
class Chat : public Menu {

	float x, y, w, h;
	std::vector<std::string> chatLog;
	Typer textRenderer;
	glm::vec4 chatColor;

	void onRender() override;

	public:

		Chat(float width, float height);
		~Chat();

		std::string currMsg;

		void addCharToCurrMsg(const char &c);
		void removeCharFromCurrMsg();
		void updateChatlog(const std::string &str);
};

#endif
