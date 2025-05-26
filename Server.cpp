#include "Server.hpp"

Server::Server() : _port(""), _sockfd_ipv4(-1), _res(NULL) {}

Server::~Server() {
	delete _client;
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
}

void Server::init(const std::string& port) {
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
	struct sockaddr_storage client_addr;
	socklen_t addr_len = sizeof(client_addr);
	//each client recives a new socketfd, accept is blocking, later will use poll or select
	int client_sockfd = accept(_sockfd_ipv4, (struct sockaddr*)&client_addr, &addr_len);

	if (client_sockfd == -1) {
		throw std::runtime_error("accept error: " + std::string(strerror(errno)));
	}

	_client = new Client(client_sockfd);
	_client->handle();

	close(client_sockfd);
	delete _client;
	_client = NULL;
}
