
#include <algorithm>
#include <chrono>
#include "Chat.hpp"

Chat::Chat(float width, float height) : Menu(width, height)
{
	DESIGN_WIDTH = 3840;
	DESIGN_HEIGHT = 2160;
	build();
	chatColor = glm::vec4(0,0,0,0.6f);
}

Chat::~Chat()
{
}

void Chat::build()
{
	x = 0.0;
	y = 0.0f;
	w = menuWidth / 3.0f;
	h =  menuHeight / 3.0f;
	textRenderer.setProjection(fullscreenWidth, fullscreenHeight);
}

void Chat::cleanMsgSent()
{
	personalChatLog.push_back(currMsg);
	currentChatLogIndex = personalChatLog.size();
	currMsg.clear();
}

void Chat::goThroughChatLog(const int key)
{
	if (personalChatLog.empty())
    	return;

	if (key == GLFW_KEY_UP && currentChatLogIndex > 0)
    	currentChatLogIndex = std::max<size_t>(currentChatLogIndex - 1, 0);
	else if (key == GLFW_KEY_DOWN)
		currentChatLogIndex = std::min(currentChatLogIndex + 1, personalChatLog.size() - 1);

	currMsg = personalChatLog[currentChatLogIndex];
}

void Chat::onRender()
{
	drawSimpleQuad(x, y, w, h, chatColor);
	float offset = 10.0f * textRenderer.getScale();
	float charHeight = 48 * textRenderer.getScale();

	textRenderer.renderText(currMsg, x + offset, y + offset, glm::vec3(0.5, 0.8f, 0.2f));

	float currHeight = offset + charHeight;
    for (auto it = chatLog.rbegin(); it != chatLog.rend() && currHeight < (h - charHeight); ++it)
	{	
		textRenderer.renderText(it->message.c_str(), x+offset, y+currHeight, glm::vec3(1.0f));
		currHeight += charHeight;
    }
}

void Chat::renderRecentMessages()
{
	auto timepoint = std::chrono::system_clock::now();
	auto nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(timepoint.time_since_epoch()).count();
	auto nowS = std::chrono::duration_cast<std::chrono::seconds>(timepoint.time_since_epoch()).count();

	float offset = 10.0f * textRenderer.getScale();
	float charHeight = 48 * textRenderer.getScale();
	float currHeight = offset + charHeight;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);

	for (auto msg = chatLog.rbegin();
		msg != chatLog.rend() && (nowS - (msg->time/1000)) < 6;
		++msg)
	{	
		double age = nowMs - msg->time;		// milliseconds since message

		float alpha = static_cast<float>(
			std::clamp(1.0 - age / (MESSAGE_LIFETIME * 1000), 0.0, 1.0)
		);

		drawSimpleQuad(x + offset, y + currHeight - offset / 2.0f - 4, textRenderer.getPixelSizeOfString(msg->message) + 2, charHeight, glm::vec4(0,0,0,alpha/2.0f));
	
		textRenderer.renderText(
			msg->message.c_str(),
			x + offset,
			y + currHeight,
			glm::vec3(1.0f),
			alpha
		);

		currHeight += charHeight;
	}

	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
}

void Chat::addCharToCurrMsg(const char &c)
{
	if (textRenderer.getPixelSizeOfString(currMsg) + textRenderer.getPixelSizeOfString("0") > (w - 10.0f)) //offset 1 more character.
		return ;
	currMsg.push_back(c);
}

void Chat::removeCharFromCurrMsg()
{
	if (currMsg.empty()) return ;
	currMsg.pop_back();
}

void Chat::updateChatlog(const std::string& str)
{
	auto now = duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count();

    chatLog.push_back({ str, now});
}