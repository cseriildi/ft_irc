#include "Client.hpp"
#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>

Client::Client(int sockfd) : _clientFd(sockfd) {}

Client::~Client() {}

// NOLINTBEGIN
std::string mockIRC(const std::string& input) {
	if (input.find("CAP LS") == 0) {
		return "CAP * LS :\r\n";
	}

	if (input.find("PING") == 0) {
		return "PONG " + input.substr(5);
	}

	return "";
}
// NOLINTEND

void Client::handle() {
	char buffer[BUFFER_SIZE];
	memset(buffer, 0, sizeof(buffer)); //NOLINT

	const ssize_t received = recv(_clientFd, buffer, sizeof(buffer) - 1, 0); //NOLINT
	if (received == 0) {
		throw std::runtime_error("Client disconnected");
	}
	if (received == -1) {
		throw std::runtime_error("Error receiving data: " + std::string(strerror(errno)));
	}

	const std::string msg(buffer, received); //NOLINT

	std::cout << "Received: " << msg;

	const std::string response = mockIRC(msg);

	if (!response.empty()) {
		if (!_sendAll(response)) {
			throw std::runtime_error("Error sending response: " + std::string(strerror(errno)));
		}
	}
}

bool Client::_sendAll(const std::string &message) const {
	size_t sent_len = 0;
	const size_t msg_len = message.length();

	while (sent_len < msg_len) {
		// send might not send all bytes at once, so we loop until all bytes are sent
		// returns the number of bytes sent, or -1 on error
		const ssize_t sent = send(_clientFd, message.c_str() + sent_len, msg_len - sent_len, 0); //NOLINT
		if (sent == -1) {
			std::cerr << "Error sending message: " << strerror(errno) << "\n";
			return false;
		}
		sent_len += sent;
	}

	return true;
}
