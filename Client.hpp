#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>
#include <cstring> // for memset
#include <cerrno>
#include <arpa/inet.h> // for send, recv

class Client {
private:
	int _sockfd_ipv4; 
	//Instance of IRC interpeter, called with a string, returns a string

	Client();
	Client(const Client &other);
	Client &operator=(const Client &other);
public:
	Client(int sockfd);
	~Client();

	void handle();
};

#endif
