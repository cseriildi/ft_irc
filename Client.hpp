#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <arpa/inet.h> // for send, recv

#define BUFFER_SIZE 512 // standard message size for IRC

class Client {
private:
	int _sockfd_ipv4;
	//Instance of IRC interpeter, called with a string, returns a string

	Client();
	Client(const Client &other);
	Client &operator=(const Client &other);

	bool _sendAll(const std::string &message) const;
public:
	Client(int sockfd);
	~Client();

	void handle();
};

#endif
