#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <netdb.h> // for getaddrinfo, freeaddrinfo
#include <unistd.h> // for close
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include "Client.hpp"

#define BACKLOG 10

class Client;

class Server {
private: // also trying normal comments
	std::string		_port; /// doxygen
	int				_sockfd_ipv4; /* normal */
	struct addrinfo	*_res; 
	Client			*_client;

	Server(const Server &other);
	Server &operator=(const Server &other);

	void cleanup();
	void bind_and_listen(const struct addrinfo *res);
public:
	Server();
	~Server();

	void init(const std::string &port);
	void run();
};

#endif
