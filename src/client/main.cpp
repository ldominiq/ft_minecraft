#include "App.hpp"
#include "UDPClient.hpp"
#include <csignal>

std::atomic<bool>& interrupted() {
	static std::atomic<bool> flag{false};
	return flag;
}

void handle_sigint(int) {
	interrupted().store(true, std::memory_order_relaxed);
}

int main(int argc, char** argv) {

	std::signal(SIGINT, handle_sigint);
	App app;
	app.run();

	return 0;
}
