#include <exception>
#include <iostream>
#include <stdexcept>

int main(int argc, char **argv) try {
	if (argc != 3)
		throw std::invalid_argument("Usage: ./ircserv <port> <password>");
	
	(void)argv;
	
	return 0;
	
} catch (const std::exception &e) {
	std::cerr << e.what() << "\n";
	return 1;
} catch (...) {
	std::cerr << "Unknown exception\n";
	return 1;
}
