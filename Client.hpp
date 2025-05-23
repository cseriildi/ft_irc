#ifndef CLIENT_HPP
#define CLIENT_HPP

class Client {
public:
	Client();
	~Client();
	Client(const Client &other);
	Client &operator=(const Client &other);
};

#endif
