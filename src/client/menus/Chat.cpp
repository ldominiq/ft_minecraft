
#include "Chat.hpp"

constexpr float scale = 0.3f;

Chat::Chat(float width, float height) : Menu(width, height), textRenderer("fonts/Roboto-Regular.ttf", scale)
{
	x = 0.0;
	y = 0.0f;// + height / 10.0f;;
	w = width / 3.0f;
	h = height / 3.0f;
	chatColor = glm::vec4(0,0,0,0.6f);
	// chatLog.push_back(std::string());
	textRenderer.setProjection(width, height);
}

Chat::~Chat()
{
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
	float offset = 10.0f;
	float charHeight = (48+10)*scale; //48 cause font is 48 and 10 is height offset

	textRenderer.renderText(currMsg, x + offset, y + offset, glm::vec3(0.5, 0.8f, 0.2f));

	int currHeight = offset + charHeight;
    for (auto it = chatLog.rbegin(); it != chatLog.rend() && currHeight < (h - charHeight); ++it)
	{	
		textRenderer.renderText(it->message.c_str(), x+offset, y+currHeight, glm::vec3(1.0f));
		currHeight += charHeight;
    }
}

void Chat::renderRecentMessages()
{
	timeval tv;
	gettimeofday(&tv, nullptr);

	float offset = 10.0f;
	float charHeight = (48+10)*scale; //48 cause font is 48 and 10 is height offset
	int currHeight = offset + charHeight;

	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glDisable(GL_DEPTH_TEST);

	for (auto msg = chatLog.rbegin();
		msg != chatLog.rend() && (tv.tv_sec - msg->time) < 6;
		++msg)
	{
		double now =
		static_cast<double>(tv.tv_sec) +
		static_cast<double>(tv.tv_usec) / 1'000'000.0;
		
		double msgTime = static_cast<double>(msg->time);

		double age = now - msgTime;        // seconds since message
		double lifetime = 6.0;             // seconds

		float alpha = static_cast<float>(
			std::clamp(1.0 - age / lifetime, 0.0, 1.0)
		);

		drawSimpleQuad(x + offset, y + currHeight - 4, textRenderer.getPixelSizeOfString(msg->message) + 2, charHeight, glm::vec4(0,0,0,alpha/2.0f));
	
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
    timeval tv;
    gettimeofday(&tv, nullptr);

    chatLog.push_back({ str, tv.tv_sec });
}