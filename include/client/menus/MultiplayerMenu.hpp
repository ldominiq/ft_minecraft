#ifndef MULTIPLAYERMENU_HPP
#define MULTIPLAYERMENU_HPP

#include "Menu.hpp"
#include <functional>

class MultiplayerMenu : public Menu {
public:
	MultiplayerMenu(float width, float height, GLuint dirtTex);

	void setConnectCallback(std::function<void(const std::string&)> cb) { onConnect = std::move(cb); }
	void setCancelCallback(std::function<void()> cb) { onCancel = std::move(cb); }

	void addChar(char c);
	void removeChar();
	std::string getIpAddress() const { return ipAddress; }

	void setErrorMessage(const std::string& msg) { errorMessage = msg; }
	void clearError() { errorMessage.clear(); }

	void handleMouseClick(double mouseX, double mouseY, int button, int action) override;
	void handleMouseMove(double mouseX, double mouseY) override;

private:
	void onRender() override;
	void build() override;

	std::function<void(const std::string&)> onConnect;
	std::function<void()> onCancel;

	GLuint dirtTexture = 0;
	std::string ipAddress = "127.0.0.1";
	std::string errorMessage;

	float inputBoxX = 0, inputBoxY = 0, inputBoxW = 0, inputBoxH = 0;
	Button connectButton;
	Button cancelButton;
};

#endif
