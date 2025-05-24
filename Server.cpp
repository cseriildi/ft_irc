#include "Server.hpp"
#include <iostream>
#include <sstream>
#include <cstring>
#include <unistd.h>
#include <cerrno>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>

Server::Server() : _port(""), _sockfd_ipv4(-1), _res(NULL) {}

Server::~Server() {
	delete _client;
	cleanup();
}

void Server::cleanup() {
	if (_res) {
		freeaddrinfo(_res);
		_res = NULL;
	}
	if (_sockfd_ipv4 != -1) {
		close(_sockfd_ipv4);
		_sockfd_ipv4 = -1;
	}
}

void Server::init(const std::string& port) {
	_port = port;

	struct addrinfo hints;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; // IPv4 only
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	int status = getaddrinfo(NULL, _port.c_str(), &hints, &_res);
	if (status != 0) {
		throw std::runtime_error("getaddrinfo error: " + std::string(gai_strerror(status)));
	}

	for (struct addrinfo* p = _res; p != NULL; p = p->ai_next) {
		try {
			bind_and_listen(p);
			break;
		} catch (const std::runtime_error& e) {
			cleanup();
			std::cerr << "Bind/listen error: " << e.what() << std::endl;
			continue;
		}
	}
}

void Server::bind_and_listen(const struct addrinfo* res) {
	int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd == -1) {
		std::ostringstream oss;
		oss << "socket error on port " << _port << ": " << strerror(errno);
		throw std::runtime_error(oss.str());
	}

	int yes = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
		close(sockfd);
		std::ostringstream oss;
		oss << "bind error on port " << _port << ": " << strerror(errno);
		throw std::runtime_error(oss.str());
	}

	if (listen(sockfd, BACKLOG) == -1) {
		close(sockfd);
		std::ostringstream oss;
		oss << "listen error on port " << _port << ": " << strerror(errno);
		throw std::runtime_error(oss.str());
	}

	_sockfd_ipv4 = sockfd;

	char ipstr[INET_ADDRSTRLEN];
	struct sockaddr_in* ipv4 = (struct sockaddr_in*)res->ai_addr;
	inet_ntop(AF_INET, &(ipv4->sin_addr), ipstr, sizeof(ipstr));
	std::cout << "Listening on IPv4: " << ipstr << std::endl;
}

void Server::run() {
	std::cout << "Server is running on port " << _port << std::endl;

	struct sockaddr_storage client_addr;
	socklen_t addr_len = sizeof(client_addr);
	int client_sockfd = accept(_sockfd_ipv4, (struct sockaddr*)&client_addr, &addr_len);

	if (client_sockfd == -1) {
		throw std::runtime_error("accept error: " + std::string(strerror(errno)));
	}

	std::cout << "Client connected" << std::endl;

	_client = new Client(client_sockfd);
	_client->handle();

	close(client_sockfd);
	delete _client;
	_client = NULL;
}
