#include "Client.hpp"

Client::Client(int sockfd) : _sockfd_ipv4(sockfd) {}

Client::~Client() {}

void Client::handle() {
	char buffer[512]; //standard message size for irc
	std::memset(buffer, 0, sizeof(buffer));

	//recive on the saved socket
	ssize_t received = recv(_sockfd_ipv4, buffer, sizeof(buffer) - 1, 0);
	if (received <= 0) {
		std::cout << "Client disconnected or error" << std::endl;
		return;
	}

	std::cout << "Received: " << buffer << std::endl;

	//send a welcome message on the socket
	std::string welcome = "NOTICE AUTH :Connected to test IRC server\r\n";
	send(_sockfd_ipv4, welcome.c_str(), welcome.length(), 0);
}
