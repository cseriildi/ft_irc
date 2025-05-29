#include "Server.hpp"
#include <arpa/inet.h>
#include <asm-generic/socket.h>
#include <cerrno>
#include <cstddef>
#include <cstring>
#include <exception>
#include <map>
#include <netdb.h>
#include <netinet/in.h>
#include <stdexcept>
#include <string>
#include <iostream>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

Server::Server(const std::string& port) : _port(port), _sockfd_ipv4(-1), _sockfd_ipv6(-1), _res(NULL) {

	struct addrinfo hints = {}; //create hints struct for getaddrinfo
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; //AF_INET for IPv4 only, AF_INET6 for IPv6, AF_UNSPEC for both
	hints.ai_socktype = SOCK_STREAM; //TCP
	hints.ai_flags = AI_PASSIVE; //localhost address

	//Create a linked list of adresses available fon the port
	const int status = getaddrinfo(NULL, _port.c_str(), &hints, &_res);
	if (status != 0) {
		//gai_strerror: getadressinfo to error string
		throw std::runtime_error("getaddrinfo error: " + std::string(gai_strerror(status)));
	}

	for (struct addrinfo* p = _res; p != NULL; p = p->ai_next) {
		try {
			if (p->ai_family == AF_INET) {
				_sockfd_ipv4 = _bind_and_listen(p);
				std::cout << "Server is listening on port " << _port << " (IPv4)\n";
			} else if (p->ai_family == AF_INET6) {
				_sockfd_ipv6 = _bind_and_listen(p);
				std::cout << "Server is listening on port " << _port << " (IPv6)\n";
			}
		} catch (const std::runtime_error& e) {
			_cleanup();
			std::cerr << "Bind/listen error: " << e.what() << "\n";
			continue;
		}
	}
}

Server::~Server() {
	_cleanup();
}

void Server::_cleanup() {
	if (_res != 0) {
		freeaddrinfo(_res); //free the linked list, from netdb.h
		_res = NULL;
	}
	if (_sockfd_ipv4 != -1) {
		close(_sockfd_ipv4);
		_sockfd_ipv4 = -1;
	}
	std::map<int, Client*>::iterator it;
	for (it = _clients.begin(); it != _clients.end(); ++it) {
		delete it->second;
	}
	_clients.clear();
}

int Server::_bind_and_listen(const struct addrinfo* res) {
	//Create a socket we can bind to, uses the nodes from getaddrinfo
	const int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd == -1) {
		throw std::runtime_error("socket error: " + std::string(strerror(errno)));
	}

	int yes = 1;
	//SOL_SOCKET: socket level, SO_REUSEADDR: option to reuse the address, yes: enable it
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
	if (res->ai_family == AF_INET6) {
		// For IPv6, we also set the IPV6_V6ONLY option to allow dual-stack sockets
		setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes));
	}

	//Bind the socket to the address and port
	if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
		close(sockfd);
		throw std::runtime_error("bind error: " + std::string(strerror(errno)));
	}

	//Listen for incoming connections
	if (listen(sockfd, BACKLOG) == -1) {
		close(sockfd);
		throw std::runtime_error("listen error: " + std::string(strerror(errno)));
	}

	return sockfd;
}

void Server::run() {
	// add server socket to pollfds
	_addPollFd(_sockfd_ipv4, POLLIN);
	_addPollFd(_sockfd_ipv6, POLLIN);

	while (true) {
		const int n_poll = poll(_pollfds.data(), _pollfds.size(), TIMEOUT);

		if (n_poll == -1) {
			std::cerr << "Poll error: " << strerror(errno) << "\n";
			continue;
		}

		for (size_t i = 0; i < _pollfds.size(); ++i) {
			// If the returned events POLLIN bit is set, there is data to read
			if ((_pollfds[i].revents & POLLIN) != 0) {
				// if the socket is still the server socket, it has not been accept()-ed yet
				if (_pollfds[i].fd == _sockfd_ipv4) {
					_handleNewConnection();
				} else {
					if (!_handleClientActivity(i)) {
						--i;
					}
				}
			}
		}
	}
}

void Server::_addPollFd(int fd, short events) {
	struct pollfd pfd = {};
	pfd.fd = fd;
	pfd.events = events;
	pfd.revents = 0; //return events, to be filled by poll
	_pollfds.push_back(pfd);
}

void Server::_handleNewConnection() {
	struct sockaddr_storage client_addr = {};
	socklen_t addrLen = sizeof(client_addr);
	// should not block now, since poll tells us there is a connection pending
	int const client_fd = accept(_sockfd_ipv4, (struct sockaddr*)&client_addr, &addrLen); //NOLINT

	if (client_fd == -1) {
		std::cerr << "Accept error: " << strerror(errno) << "\n";
		return;
	}

	std::cout << "New client connected: " << client_fd << "\n";
	_clients[client_fd] = new Client(client_fd);
	_addPollFd(client_fd, POLLIN);
}

bool Server::_handleClientActivity(size_t index) {
	int const client_fd = _pollfds[index].fd;
	Client* client = _clients[client_fd];

	if (client == 0)
		return true;

	try {
		client->handle();
	} catch (const std::exception& e) {
		std::cerr << "Client error on fd " << client_fd << ": " << e.what() << "\n";
		_removeClient(index, client_fd);
		return false;
	}
	return true;
}

void Server::_removeClient(size_t index, int fd) {
	close(fd); // Close the file descriptor to prevent leaks
	delete _clients[fd];
	_clients.erase(fd);
	_pollfds.erase(_pollfds.begin() + index); //NOLINT
}
