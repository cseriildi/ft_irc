#include <csignal>
#include <exception>
#include <iostream>
#include <signal.h>
#include <stdexcept>
#include "Server.hpp"

// use socat -v TCP-LISTEN:6667,reuseaddr,fork TCP:127.0.0.1:6668 for proxy
volatile sig_atomic_t g_terminate = 0; //NOLINT

void handle_signal(int signum) { //NOLINT
	(void)signum;
	g_terminate = 1;
}

int main(int argc, char **argv) try {
	if (argc != 3)
		throw std::invalid_argument("Usage: ./ircserv <port> <password>");

	signal(SIGINT, handle_signal); //NOLINT
	signal(SIGQUIT, handle_signal); //NOLINT
	Server server(argv[1], argv[2]); //NOLINT
	server.run();

	return 0;

} catch (const std::exception &e) {
	std::cerr << e.what() << "\n";
	return 1;
} catch (...) {
	std::cerr << "Unknown exception\n";
	return 1;
}
