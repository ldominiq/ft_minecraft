#include <iostream>
#include "Server.hpp"
#include <csignal>

Server* g_server = nullptr;

void handle_sigint(int) {
    if (g_server) {
        g_server->saveWorldOnExit();
    }
    exit(0);
}

int main(int argc, char* argv[]) {

	std::optional<int> seed;
	if (argc > 1)
	{
		try {
			seed = std::stoi(argv[1]);
		} catch (std::exception &e) {
			std::cout << e.what() << ": please. Just put a number... exiting" << std::endl;
			exit(1);
		}
	}
	
	std::signal(SIGINT, handle_sigint);
	Server serv;
	g_server = &serv;
    g_server->run(seed);
}
