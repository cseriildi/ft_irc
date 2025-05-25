#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <iostream>
#include <cstring> // for memset
#include <cerrno>
#include <arpa/inet.h> // for send, recv

/**
 * @brief Client class to handle communication with a connected client.
 *
 * @details This class is responsible for managing a single client connection.
 */
class Client {
private:
	int _sockfd_ipv4; ///< Socket fd for IPv4
	//Instance of IRC interpeter, called with a string, returns a string

	/**
	 * @brief Default constructor. deleted to prevent creation without a socket fd
	 */
	Client();
	/**
	 * @brief Copy constructor. deleted to prevent duplicate fds
	 */
	Client(const Client &other);
	/**
	 * @brief Assignment operator. deleted to prevent duplicate fds
	 */
	Client &operator=(const Client &other);
public:
	/**
	 * @brief Constructor that initializes the client with a socket fd.
	 *
	 * @param sockfd The socket file descriptor for the client connection.
	 */
	Client(int sockfd);
	/**
	 * @brief Default destructor.
	 */
	~Client();

	/**
	 * @brief Handles communication with the client.
	 *
	 * @details Receives a message from the client, processes it, and sends a response.
	 * 		For now, it simply echoes back a welcome message.
	 */
	void handle();
};

#endif
