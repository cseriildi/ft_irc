#include <exception>
#include <iostream>
#include <stdexcept>
#include "Server.hpp"

// use socat -v TCP-LISTEN:6667,reuseaddr,fork TCP:127.0.0.1:6668 for proxy

int main(int argc, char **argv) try {
	if (argc != 3)
		throw std::invalid_argument("Usage: ./ircserv <port> <password>");

	Server server(argv[1]);
	server.run();

	return 0;

} catch (const std::exception &e) {
	std::cerr << e.what() << "\n";
	return 1;
} catch (...) {
	std::cerr << "Unknown exception\n";
	return 1;
}
