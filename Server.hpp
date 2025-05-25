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

/**
 * @brief Server class to handle incoming connections and manage clients.
 *
 * @details Still a rudamentary implementation, this class gathers the necessary
 * 		address information, binds to the specified port, and listens for incoming
 * 		connections. It creates a Client instance for each accepted connection.
 *		For now only uses IPv4, can revice and send exactly one message, then closes the connection.
 *		Uses the blocking accept() call to wait for incoming connections.,
 */
class Server {
private:
	std::string		_port; ///< Port to listen on
	int				_sockfd_ipv4; ///< a singular ipv4 fd to listen on
	struct addrinfo	*_res; ///< linked list with address information
	Client			*_client; ///< Client instance to handle communication

	/**
	 * @brief Copy constructor. deleted to prevent duplicate fds
	 */
	Server(const Server &other);
	/**
	 * @brief Assignment operator. deleted to prevent duplicate fds
	 */
	Server &operator=(const Server &other);

	/**
	 * @brief Cleans up resources.
	 *
	 * @details Closes the socket and frees the addrinfo structure.
	 */
	void cleanup();
	/**
	 * @brief Binds the socket to the address and starts listening.
	 *
	 * @param res The addrinfo structure containing the address information.
	 * @throws std::runtime_error if binding or listening fails.
	 */
	void bind_and_listen(const struct addrinfo *res);
public:
	/**
	 * @brief Default constructor.
	 *
	 * @details Initializes the server with no port and no socket.
	 * 		I plan to move init here later
	 */
	Server(); //TODO: move init here later
	/**
	 * @brief Destructor.
	 *
	 * @details Frees the linked list and closes the socket.
	 */
	~Server();

	/**
	 * @brief Initializes the server with the specified port.
	 *
	 * @param port The port to listen on.
	 * @throws std::runtime_error if getaddrinfo fails or if binding/listening fails.
	 */
	void init(const std::string &port);
	/**
	 * @brief Runs the server, accepting incoming connections and handling clients.
	 *
	 * @details This method blocks until a client connects, then creates a Client instance
	 * 		to handle communication with that client.
	 */
	void run();
};

#endif
