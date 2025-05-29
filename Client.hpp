#pragma once

#include <string>
#include <arpa/inet.h> // for send, recv

#define BUFFER_SIZE 512 // standard message size for IRC

class Client {
	public:

		Client(int sockfd);
		~Client();

		void handle();

	private:

		//Instance of IRC interpeter, called with a string, returns a string
		Client();
		Client(const Client &other);
		Client &operator=(const Client &other);
		
		bool	_sendAll(const std::string &message) const;

		int		_clientFd;
};
