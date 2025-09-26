
#ifndef CHAT_HPP
#define CHAT_HPP

#include "Typer.hpp"
#include "Menu.hpp"

class Chat : public Menu {

	float scale = 0.3f;
	float x, y, w, h;
	std::vector<std::string> chatLog;
	Typer textRenderer;

	void onRender();

	public:

		Chat(float width, float height);
		~Chat();

		std::string currMsg;

		void addCharToCurrMsg(const char &c);
		void removeCharFromCurrMsg();
		void updateChatlog(const std::string &str);
};

#endif
