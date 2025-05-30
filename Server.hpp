#pragma once

#include "Channel.hpp"
#include "Client.hpp"
#include <cstddef>
#include <string>
#include <vector>
#include <map>

#define BACKLOG 10
#define MAX_CLIENTS 100
#define TIMEOUT 5000 // poll will block for this long unless an event occurs

class Server {
	public:

		Server(const std::string &port);
		~Server();

		void run();

	private:

		Server();
		Server(const Server &other);
		Server &operator=(const Server &other);

		void		_cleanup();
		static int	_bindAndListen(const struct addrinfo *res);
		void		_addPollFd(int fd, short events);
		void		_handleNewConnection(int sockfd);
		bool		_handleClientActivity(size_t index);
		void		_removeClient(size_t index, int cfd);
		void		_handlePollEvents();
		void		_sendToClient(Client *client, const std::string &msg);
		void		_sendToChannel(Channel *channel, const std::string &msg, Client *sender = NULL);

		std::string						_port;
		int								_sockfdIpv4;
		int								_sockfdIpv6;
		struct addrinfo*				_res;
		std::vector<struct pollfd>		_pollFds; // vector of the fds we are polling
		std::map<int, Client*>			_clients; // with client_fd as key
		std::map<std::string, Channel*>	_channels; // with channel name as key
};
