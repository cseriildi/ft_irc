#ifndef SERVER_HPP
#define SERVER_HPP

#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include "Client.hpp"

#define BACKLOG 10
#define MAX_CLIENTS 100
#define TIMEOUT 5000 // poll will block for this long unless an event occurs

class Client;

class Server {
private:
	std::string		_port;
	int				_sockfd_ipv4;
	int				_sockfd_ipv6;
	struct addrinfo	*_res;
	std::vector<struct pollfd> _pollfds; // vector of the fds we are polling
	std::map<int, Client*> _clients; // with client_fd as key

	Server();
	Server(const Server &other);
	Server &operator=(const Server &other);

	void _cleanup();
	static int _bind_and_listen(const struct addrinfo *res);
	void _addPollFd(int fd, short events);
	void _handleNewConnection();
	bool _handleClientActivity(size_t index);
	void _removeClient(size_t index, int cfd);
public:
	Server(const std::string &port);
	~Server();

	void run();
};

#endif
