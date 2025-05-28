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
		throw std::runtime_error("Error receiving data: " + std::string(strerror(errno)));
	}

	std::string msg(buffer);

	std::cout << "Received: " << msg;

	std::string response = mockIRC(msg);

	if (!response.empty()) {
		if (!_sendAll(response)) {
			throw std::runtime_error("Error sending response: " + std::string(strerror(errno)));
		}
	}
}


bool Client::_sendAll(const std::string &message) {
	size_t sent_len = 0;
	size_t msg_len = message.length();

	while (sent_len < msg_len) {
		// send might not send all bytes at once, so we loop until all bytes are sent
		// returns the number of bytes sent, or -1 on error
		ssize_t sent = send(_sockfd_ipv4, message.c_str() + sent_len, msg_len - sent_len, 0);
		if (sent == -1) {
			std::cerr << "Error sending message: " << strerror(errno) << std::endl;
			return false;
		}
		sent_len += sent;
	}

	return true;
}
