#include "Channel.hpp"

#include <cstddef>
#include <map>
#include <string>

#include "Client.hpp"

Channel::Channel(const std::string &name, Server *server)
    : _name(name),
      _isInviteOnly(false),
      _topicSet(false),
      _passRequired(false),
      _isLimited(false),
      _limit(0),
      _server(server) {}

Channel::~Channel() {}

std::map<int, Client *> Channel::getClients() const { return _clients; }
std::map<int, Client *> Channel::getOperators() const { return _operators; }
std::string Channel::getName() const { return _name; }
bool Channel::isInviteOnly() const { return _isInviteOnly; }
bool Channel::isTopicSet() const { return _topicSet; }
bool Channel::isPassRequired() const { return _passRequired; }
bool Channel::isLimited() const { return _isLimited; }
int Channel::getLimit() const { return _limit; }

void Channel::addClient(Client *client) {
  if (client == NULL) {
    return;
  }
  _clients[client->getFd()] = client;
}

void Channel::removeClient(int clientFd) {
  const std::map<int, Client *>::iterator it = _clients.find(clientFd);
  if (it != _clients.end()) {
    _clients.erase(it);
  }
  if (_clients.empty()) {
    _server->removeChannel(_name);
    return;
  }
  removeOperator(clientFd);
}

void Channel::addOperator(Client *client) {
  if (client == NULL) {
    return;
  }
  _operators[client->getFd()] = client;
}

void Channel::removeOperator(int clientFd) {
  const std::map<int, Client *>::iterator it = _operators.find(clientFd);
  if (it != _operators.end()) {
    _operators.erase(it);
  }
  if (_operators.empty()) {
    _operators[_clients.begin()->first] = _clients.begin()->second;
    // Not sure this is the right user to promote to operator
  }
}
