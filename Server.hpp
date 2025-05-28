#ifndef SERVER_HPP
#define SERVER_HPP

#include <iostream>
#include <netdb.h> // for getaddrinfo, freeaddrinfo
#include <unistd.h> // for close
#include <cstring>
#include <cerrno>
#include <stdexcept>
#include <vector>
#include <map>
#include <poll.h>
#include "Client.hpp"

#define BACKLOG 10
#define MAX_CLIENTS 100
#define TIMEOUT 5000 // poll will block for this long unless an event occurs

class Client;

class Server {
private:
	std::string		_port;
	int				_sockfd_ipv4;
	struct addrinfo	*_res;
	std::vector<struct pollfd> _pollfds; // vector of the fds we are polling
	std::map<int, Client*> _clients; // with client_fd as key

	Server();
	Server(const Server &other);
	Server &operator=(const Server &other);

	void cleanup();
	void bind_and_listen(const struct addrinfo *res);
	void addPollFd(int fd, short events);
	void handleNewConnection();
	bool handleClientActivity(size_t index);
	void removeClient(size_t index, int cfd);
public:
	Server(const std::string &port);
	~Server();

	void run();
};

#endif
