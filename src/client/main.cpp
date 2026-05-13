#include "App.hpp"
#include "UDPClient.hpp"
#include <csignal>

App* g_app = nullptr;

void handle_sigint(int) {
    if (g_app) {
        g_app->cleanup();
    }

	GLFWwindow* w = g_app ? g_app->getWindow() : nullptr;
	if (w)
    	glfwSetWindowShouldClose(w, GLFW_TRUE);
	else
		std::exit(0);
}

int main(int argc, char** argv) {

	std::signal(SIGINT, handle_sigint);
	App app;
	g_app = &app;
	g_app->run();

	return 0;
}
