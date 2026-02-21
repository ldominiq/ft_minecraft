
#ifndef CHAT_HPP
#define CHAT_HPP

#include "Typer.hpp"
#include "Menu.hpp"
#include "sys/time.h"

//menus.. and everything really should use an EBO (indexes) to spare vertices... but it's kinda whatever at this point.

struct ChatLine
{
	std::string message;
	time_t time;
};

class Chat : public Menu {

	float x, y, w, h;
	std::vector<ChatLine> chatLog; //takes every message
	std::vector<std::string> personalChatLog; //only takes messages sent by client
	Typer textRenderer;
	glm::vec4 chatColor;

	void onRender() override;
	
	size_t currentchatLogIndex = 0;

	public:

		Chat(float width, float height);
		~Chat();

		std::string currMsg;

		void cleanMsgSent();
		void goThroughchatLog(const int key);
		void addCharToCurrMsg(const char &c);
		void removeCharFromCurrMsg();
		void updateChatlog(const std::string &str);
		void renderRecentMessages();
};

#endif
