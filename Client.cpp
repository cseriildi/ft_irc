#include "Client.hpp"
#include <cerrno>
#include <iostream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>

Client::Client(int sockfd, Server* server) : _clientFd(sockfd),  _server(server) {}

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

void Client::receive() {
	char buffer[BUFFER_SIZE];
	memset(buffer, 0, sizeof(buffer)); //NOLINT

	const ssize_t received = recv(_clientFd, buffer, sizeof(buffer) - 1, 0); //NOLINT
	if (received == 0) {
		throw std::runtime_error("Client disconnected");
	}
	if (received == -1) {
		throw std::runtime_error("Error receiving data: " + std::string(strerror(errno)));
	}
	_inBuffer.append(buffer, received);
	std::string response = mockIRC(_inBuffer);
	if (!response.empty()) {
		_outBuffer += response;
	}
}

void Client::handle() {

	//TODO: PASS
	//TODO: NICK
	//TODO: WHO
	//TODO: USER
	//TODO: JOIN
	//TODO: PRIVMSG
	//TODO: PING
	//TODO: MODE
	//TODO: TOPIC
	//TODO: KICK
	//TODO: INVITE
	//TODO: PART

	//TODO: broadcast if in channel


}

void Client::answer() {
	while (!_outBuffer.empty()) {
		const ssize_t sent = send(_clientFd, _outBuffer.c_str(), _outBuffer.length(), 0); //NOLINT
		if (sent == -1) {
			throw std::runtime_error("Error sending data: " + std::string(strerror(errno)));
		}
		_outBuffer.erase(0, sent);
	}
}

bool Client::wantsToWrite() const {
	return !_outBuffer.empty();
}
