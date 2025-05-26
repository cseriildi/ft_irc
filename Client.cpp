#include "Client.hpp"

Client::Client(int sockfd) : _sockfd_ipv4(sockfd) {}

Client::~Client() {}

std::string mockIRC(const std::string& input) {
	if (input.find("CAP LS") == 0) {
		return "CAP * LS :\r\n";
	}

	if (input.find("PING") == 0) {
		return "PONG " + input.substr(5);
	}

	return "";
}

void Client::handle() {
	char buffer[512]; //standard message size for irc
	std::memset(buffer, 0, sizeof(buffer));
	ssize_t received = recv(_sockfd_ipv4, buffer, sizeof(buffer) - 1, 0);
	if (received <= 0) {
		std::cout << "Client disconnected or error" << std::endl;
		return;
	}

	std::string msg(buffer);

	std::cout << "Received: " << msg;

	std::string response = mockIRC(msg);

	if (!response.empty()) {
		if (!_sendAll(response)) {
			std::cerr << "Failed to send response." << std::endl;
			return;
		}
	}
}


bool Client::_sendAll(const std::string &message) {
	size_t sentLen = 0;
	size_t msgLen = message.length();

	while (sentLen < msgLen) {
		// send might not send all bytes at once, so we loop until all bytes are sent
		// returns the number of bytes sent, or -1 on error
		ssize_t sent = send(_sockfd_ipv4, message.c_str() + sentLen, msgLen - sentLen, 0);
		if (sent == -1) {
			std::cerr << "Error sending message: " << strerror(errno) << std::endl;
			return false;
		}
		sentLen += sent;
	}

	return true;
}
