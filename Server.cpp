#include "Server.hpp"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cerrno>
#include <csignal>
#include <cstddef>
#include <cstring>
#include <ctime>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

#include "Channel.hpp"
#include "Client.hpp"
#include "utils.hpp"

extern volatile sig_atomic_t g_terminate;  // NOLINT

const std::map<Server::ERR, std::string> Server::ERRORS = init_error_map();

std::map<Server::ERR, std::string> Server::init_error_map() {
  std::map<ERR, std::string> errorMap;
  errorMap[ERR_NOSUCHNICK] = "No such nick/channel";
  errorMap[ERR_NOSUCHSERVER] = "No such server";
  errorMap[ERR_NOSUCHCHANNEL] = "No such channel";
  errorMap[ERR_CANNOTSENDTOCHAN] = "Cannot send to channel";
  errorMap[ERR_TOOMANYTARGETS] = "Duplicate recipients. No message delivered";
  errorMap[ERR_NOORIGIN] = "No origin specified";
  errorMap[ERR_NORECIPIENT] = "No recipient given";
  errorMap[ERR_NOTEXTTOSEND] = "No text to send";
  errorMap[ERR_NOTOPLEVEL] = "No toplevel domain specified";
  errorMap[ERR_WILDTOPLEVEL] = "Wildcard in toplevel domain";
  errorMap[ERR_UNKNOWNCOMMAND] = "Unknown command";
  errorMap[ERR_NONICKNAMEGIVEN] = "No nickname given";
  errorMap[ERR_ERRONEUSNICKNAME] = "Erroneous nickname";
  errorMap[ERR_NICKNAMEINUSE] = "Nickname is already in use";
  errorMap[ERR_USERNOTINCHANNEL] = "They aren't on that channel";  // TODO
  errorMap[ERR_NOTONCHANNEL] = "You're not on that channel";
  errorMap[ERR_USERONCHANNEL] = "User already on channel";
  errorMap[ERR_NOTREGISTERED] = "You have not registered";
  errorMap[ERR_NEEDMOREPARAMS] = "Not enough parameters";
  errorMap[ERR_ALREADYREGISTRED] = "You are already registered";
  errorMap[ERR_PASSWDMISMATCH] = "Password mismatch";
  errorMap[ERR_KEYSET] = "Channel key already set";
  errorMap[ERR_CHANNELISFULL] = "Cannot join channel (+l)";
  errorMap[ERR_UNKNOWNMODE] = "is unknown mode char to me for";  // TODO
  errorMap[ERR_INVITEONLYCHAN] = "Cannot join channel (+i)";
  errorMap[ERR_BADCHANNELKEY] = "Cannot join channel (+k)";
  errorMap[ERR_CHANOPRIVSNEEDED] = "You're not channel operator";
  return errorMap;
}

Server::Server(const std::string &port, const std::string &pass)
    : _port(port),
      _sockfdIpv4(-1),
      _sockfdIpv6(-1),
      _res(NULL),
      _name("localhost"),  // TODO
      _password(pass),
      _createdAt(std::time(NULL)) {
  _isPassRequired = !_password.empty();  // TODO: think about it
  struct addrinfo hints = {};            // create hints struct for getaddrinfo
  std::memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_UNSPEC;      // AF_INET for IPv4 only, AF_INET6 for IPv6,
                                    // AF_UNSPEC for both
  hints.ai_socktype = SOCK_STREAM;  // TCP
  hints.ai_flags = AI_PASSIVE;      // localhost address

  // Create a linked list of adresses available fon the port
  const int status = getaddrinfo(NULL, _port.c_str(), &hints, &_res);
  if (status != 0) {
    // gai_strerror: getadressinfo to error string
    throw std::runtime_error("getaddrinfo error: " +
                             std::string(gai_strerror(status)));
  }

  for (struct addrinfo *p = _res; p != NULL; p = p->ai_next) {
    try {
      if (p->ai_family == AF_INET) {
        _sockfdIpv4 = _bindAndListen(p);
        std::cout << "Server is listening on port " << _port << " (IPv4)\n";
      } else if (p->ai_family == AF_INET6) {
        _sockfdIpv6 = _bindAndListen(p);
        std::cout << "Server is listening on port " << _port << " (IPv6)\n";
      }
    } catch (const std::runtime_error &e) {
      _cleanup();
      std::cerr << "Bind/listen error: " << e.what() << "\n";
      continue;
    }
  }
}

Server::~Server() { _cleanup(); }

void Server::_cleanup() {
  std::cout << "Cleaning up server resources...\n";
  if (_sockfdIpv4 != -1) {
    std::cout << "Closing IPv4 socket: " << _sockfdIpv4 << "\n";
    close(_sockfdIpv4);
    _sockfdIpv4 = -1;
  }
  if (_sockfdIpv6 != -1) {
    std::cout << "Closing IPv6 socket: " << _sockfdIpv6 << "\n";
    close(_sockfdIpv6);
    _sockfdIpv6 = -1;
  }
  if (_res != 0) {
    freeaddrinfo(_res);  // free the linked list, from netdb.h
    _res = NULL;
  }
  ClientList::iterator it;
  for (it = _clients.begin(); it != _clients.end(); ++it) {
    close(it->first);
    delete it->second;
  }
  _clients.clear();
}

int Server::_bindAndListen(const struct addrinfo *res) {
  // Create a socket we can bind to, uses the nodes from getaddrinfo
  const int sockfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
  if (sockfd == -1) {
    throw std::runtime_error("socket error: " + std::string(strerror(errno)));
  }

  int yes = 1;
  // SOL_SOCKET: socket level, SO_REUSEADDR: option to reuse the address, yes:
  // enable it
  setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));
  if (res->ai_family == AF_INET6) {
    // For IPv6, we also set the IPV6_V6ONLY option to allow dual-stack sockets
    setsockopt(sockfd, IPPROTO_IPV6, IPV6_V6ONLY, &yes, sizeof(yes));
  }

  // Bind the socket to the address and port
  if (bind(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
    close(sockfd);
    throw std::runtime_error("bind error: " + std::string(strerror(errno)));
  }

  // Listen for incoming connections
  if (listen(sockfd, BACKLOG) == -1) {
    close(sockfd);
    throw std::runtime_error("listen error: " + std::string(strerror(errno)));
  }

  return sockfd;
}

void Server::run() {
  // add server socket to pollfds
  _addPollFd(_sockfdIpv4, POLLIN);
  _addPollFd(_sockfdIpv6, POLLIN);

  while (g_terminate == 0) {
    const int n_poll = poll(_pollFds.data(), _pollFds.size(), TIMEOUT);

    if (n_poll == -1) {
      if (errno != EINTR)
        std::cerr << "Poll error: " << strerror(errno) << "\n";
      continue;
    }

    _handlePollEvents();
  }
  _cleanup();
}

void Server::_handlePollEvents() {
  for (size_t i = 0; i < _pollFds.size(); ++i) {
    // If the returned events POLLIN bit is set, there is data to read
    if ((_pollFds[i].revents & POLLIN) != 0) {
      // if the socket is still the server socket, it has not been accept()-ed
      // yet
      if (_pollFds[i].fd == _sockfdIpv4 || _pollFds[i].fd == _sockfdIpv6) {
        _handleNewConnection(_pollFds[i].fd);
      } else {
        if (!_handleClientActivity(i)) {
          --i;
        }
      }
    }
    if ((_pollFds[i].revents & POLLOUT) != 0) {
      if (!_handleClientActivity(i)) {
        --i;
      }
    }
  }
}

void Server::_addPollFd(int fd, short events) {
  struct pollfd pfd = {};
  pfd.fd = fd;
  pfd.events = events;
  pfd.revents = 0;  // return events, to be filled by poll
  _pollFds.push_back(pfd);
}

void Server::_handleNewConnection(int sockfd) {
  struct sockaddr_storage client_addr = {};
  socklen_t addrLen = sizeof(client_addr);
  // should not block now, since poll tells us there is a connection pending
  int const client_fd =
      accept(sockfd, (struct sockaddr *)&client_addr, &addrLen);  // NOLINT

  if (client_fd == -1) {
    std::cerr << "Accept error: " << strerror(errno) << "\n";
    return;
  }

  std::cout << "New client connected: " << client_fd << "\n";
  _clients[client_fd] = new Client(client_fd, this);
  _addPollFd(client_fd, POLLIN);
}

bool Server::_handleClientActivity(size_t index) {
  int const client_fd = _pollFds[index].fd;
  Client *client = _clients[client_fd];

  if (client == 0) return true;

  if ((_pollFds[index].revents & (POLLHUP | POLLERR)) != 0) {
    std::cerr << "Client fd " << client_fd << " hangup or error\n";
    removeClient(client_fd);
    return false;
  }
  if ((_pollFds[index].revents & POLLIN) != 0) {
    try {
      client->receive();
      if (client->wantsToQuit()) {
        std::cout << "Client fd " << client_fd << " wants to quit\n";
        removeClient(client_fd);
        return false;
      }
    } catch (const std::runtime_error &e) {
      std::cerr << "Receive error on fd " << client_fd << ": " << e.what()
                << "\n";
      removeClient(client_fd);
      return false;
    }
  }
  if ((_pollFds[index].revents & POLLOUT) != 0) {
    try {
      client->answer();
    } catch (const std::runtime_error &e) {
      std::cerr << "Send error on fd " << client_fd << ": " << e.what() << "\n";
      removeClient(client_fd);
      return false;
    }
    if (!client->wantsToWrite()) {
      _pollFds[index].events &= ~POLLOUT;
    }
  }
  return true;
}

void Server::removeClient(int fd) {
  Client *client = findClient(_clients, fd);
  if (client == NULL) {
    return;
  }
  if (!client->wantsToQuit()) {
    client->leaveAllChannels("Client disconnected", "QUIT");
  }
  close(fd);
  delete client;
  _clients.erase(fd);
  for (std::vector<struct pollfd>::iterator it = _pollFds.begin();
       it != _pollFds.end(); ++it) {
    if (it->fd == fd) {
      _pollFds.erase(it);
      break;
    }
  }
}

void Server::sendToClient(Client *client, const std::string &msg) {
  if (client == NULL || msg.empty()) {
    return;
  }
  client->appendToOutBuffer(msg);
  for (size_t i = 0; i < _pollFds.size(); ++i) {
    if (_pollFds[i].fd == client->getClientFd()) {
      _pollFds[i].events |= POLLOUT;
      break;
    }
  }
}

void Server::sendToChannel(Channel *channel, const std::string &msg,
                           Client *sender) {
  if (channel == NULL || msg.empty()) {
    return;
  }
  ClientList clients = channel->getClients();
  for (ClientList::const_iterator it = clients.begin(); it != clients.end();
       ++it) {
    Client *client = it->second;
    if (client != sender) {
      sendToClient(client, msg);
    }
  }
}

bool Server::isNicknameAvailable(const Client *user,
                                 const std::string &nick) const {
  Client *found = findClient(_clients, nick);
  return found == NULL || found == user;
}

const std::string &Server::getName() const { return _name; }
const std::string &Server::getPort() const { return _port; }
const std::string &Server::getPassword() const { return _password; }
bool Server::isPassRequired() const { return _isPassRequired; }
const std::map<std::string, Channel *> &Server::getChannels() const {
  return _channels;
}
const ClientList &Server::getClients() const { return _clients; }
std::time_t Server::getCreatedAt() const { return _createdAt; }

void Server::addClient(Client *client) {
  if (client == NULL) {
    return;
  }
  _clients[client->getClientFd()] = client;
  _addPollFd(client->getClientFd(), POLLIN);
}

void Server::addChannel(Channel *channel) {
  if (channel == NULL) {
    return;
  }
  _channels[channel->getName()] = channel;
}

void Server::removeChannel(const std::string &name) {
  Channel *channel = findChannel(_channels, name);
  if (channel == NULL) {
    return;
  }
  delete channel;
  _channels.erase(name);
}
