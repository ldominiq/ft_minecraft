#include "App.hpp"
#include "UDPClient.hpp"

int main(int argc, char** argv) {

	std::string ip = (argc > 1) ? argv[1] : "127.0.0.1";
	App app(ip);
	app.run();

	return 0;
}
