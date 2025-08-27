#include <iostream>
#include "Server.hpp"

int main()
{
	Server serv;
	serv.run();

	// if (argc > 1)
	// {
	// 	int seed;
	// 	try {
	// 		seed = std::stoi(argv[1]);
	// 	} catch (std::exception &e) {
	// 		std::cout << e.what() << ": please. Just put a number... exiting" << std::endl;
	// 		exit(1);
	// 	}
		
	// 	App app(seed);
	// 	app.run();

	// } else {
	// 	App app;
	// 	app.run();
	// }

	return 0;
}