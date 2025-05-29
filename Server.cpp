#include "Server.hpp"

Server::Server(const std::string& port) : _sockfd_ipv4(-1), _res(NULL) {
	_port = port;

	struct addrinfo hints; //create hints struct for getaddrinfo
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET; //IPv4 only. AF_INET6 for IPv6, AF_UNSPEC for both
	hints.ai_socktype = SOCK_STREAM; //TCP
	hints.ai_flags = AI_PASSIVE; //localhost address

	//Create a linked list of adresses available fon the port
	int status = getaddrinfo(NULL, _port.c_str(), &hints, &_res);
	if (status != 0) {
		//gai_strerror: getadressinfo to error string
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

Server::~Server() {
	cleanup();
}

void Server::cleanup() {
	if (_res) {
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

void Server::bind_and_listen(const struct addrinfo* res) {
	//Create a socket we can bind to, uses the nodes from getaddrinfo
	int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
	if (sockfd == -1) {
		throw std::runtime_error("socket error: " + std::string(strerror(errno)));
	}

	int yes = 1;
	//SOL_SOCKET: socket level, SO_REUSEADDR: option to reuse the address, yes: enable it
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

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

	//Store the socket file descriptor for IPv4
	_sockfd_ipv4 = sockfd;
}

void Server::run() {
	// add server socket to pollfds
	addPollFd(_sockfd_ipv4, POLLIN);

	while (true) {
		int n_poll = poll(_pollfds.data(), _pollfds.size(), TIMEOUT);

		if (n_poll == -1) {
			std::cerr << "Poll error: " << strerror(errno) << std::endl;
			continue;
		}

		for (size_t i = 0; i < _pollfds.size(); ++i) {
			// If the returned events POLLIN bit is set, there is data to read
			if (_pollfds[i].revents & POLLIN) {
				// if the socket is still the server socket, it has not been accept()-ed yet
				if (_pollfds[i].fd == _sockfd_ipv4) {
					handleNewConnection();
				} else {
					if (!handleClientActivity(i)) {
						--i;
					}
				}
			}
		}
	}
}

void Server::addPollFd(int fd, short events) {
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = events;
	pfd.revents = 0; //return events, to be filled by poll
	_pollfds.push_back(pfd);
}

void Server::handleNewConnection() {
	struct sockaddr_storage client_addr;
	socklen_t addrLen = sizeof(client_addr);
	// should not block now, since poll tells us there is a connection pending
	int client_fd = accept(_sockfd_ipv4, (struct sockaddr*)&client_addr, &addrLen);

	if (client_fd == -1) {
		std::cerr << "Accept error: " << strerror(errno) << std::endl;
		return;
	}

	std::cout << "New client connected: " << client_fd << std::endl;
	_clients[client_fd] = new Client(client_fd);
	addPollFd(client_fd, POLLIN);
}

bool Server::handleClientActivity(size_t index) {
	int client_fd = _pollfds[index].fd;
	Client* client = _clients[client_fd];

	if (!client)
		return true;

	try {
		client->handle();
	} catch (const std::exception& e) {
		std::cerr << "Client error on fd " << client_fd << ": " << e.what() << std::endl;
		removeClient(index, client_fd);
		return false;
	}
	return true;
}

void Server::removeClient(size_t index, int fd) {
	delete _clients[fd];
	_clients.erase(fd);
	_pollfds.erase(_pollfds.begin() + index);
}
