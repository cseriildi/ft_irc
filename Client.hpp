#pragma once

#include "Message.hpp"
#include <string>
#include <arpa/inet.h> // for send, recv

#define BUFFER_SIZE 512 // standard message size for IRC

class Server;

class Client {
	public:

		Client(int sockfd, Server *server);
		~Client();

		void		handle();
		void		receive();
		void		answer();
		bool		wantsToWrite() const;
		void		appendToOutBuffer(const std::string &msg);
		int			getFd() const;

		Message nick(const std::string &msg);
		Message user(const std::string &msg);
		Message pass(const std::string &msg);
		Message who(const std::string &msg);
		Message privmsg(const std::string &msg);
		Message ping(const std::string &msg);

		// Channel commands
		Message join(const std::string &msg);
		Message part(const std::string &msg);
		Message kick(const std::string &msg);
		Message invite(const std::string &msg);
		Message topic(const std::string &msg);
		Message mode(const std::string &msg);
		Message list(const std::string &msg);
		Message names(const std::string &msg);

	private:

		//Instance of IRC interpeter, called with a string, returns a string
		Client();
		Client(const Client &other);
		Client &operator=(const Client &other);

		int			_clientFd;
		Server*		_server;
		std::string	_inBuffer;
		std::string	_outBuffer;
};
