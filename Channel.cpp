#include "Channel.hpp"

#include <cstddef>
#include <map>
#include <string>

#include "Client.hpp"
#include "Server.hpp"
#include "utils.hpp"

Channel::Channel(const std::string &name, Server *server)
    : _name(name),
      _isInviteOnly(false),
      _topicSet(false),
      _passRequired(false),
      _isLimited(false),
      _limit(0),
      _server(server) {}

Channel::~Channel() {}

ClientList Channel::getClients() const { return _clients; }
ClientList Channel::getOperators() const { return _operators; }
ClientList Channel::getInvited() const { return _invited; }
std::string Channel::getName() const { return _name; }
bool Channel::isInviteOnly() const { return _isInviteOnly; }
bool Channel::isTopicSet() const { return _topicSet; }
bool Channel::isPassRequired() const { return _passRequired; }
bool Channel::isLimited() const { return _isLimited; }
size_t Channel::getLimit() const { return _limit; }
std::string Channel::getTopic() const { return _topic; }
std::string Channel::getPassword() const { return _password; }
void Channel::setTopic(const std::string &topic) {
  _topic = topic;
  _topicSet = true;
}
void Channel::setPassword(const std::string &password) {
  _password = password;
  _passRequired = true;
}
void Channel::setInviteOnly(bool inviteOnly) { _isInviteOnly = inviteOnly; }
void Channel::setTopicSet(bool topicSet) { _topicSet = topicSet; }
void Channel::setPassRequired(bool passRequired) {
  _passRequired = passRequired;
}
void Channel::setPass(const std::string &pass) {
  _password = pass;
  _passRequired = true;
}
void Channel::setLimited(bool limited) { _isLimited = limited; }
void Channel::setLimit(size_t limit) {
  _limit = limit;
  _isLimited = true;
}

void Channel::addClient(Client *client) {
  if (client == NULL) {
    return;
  }
  _clients[client->getFd()] = client;
  if (_operators.empty()) {
    _operators[client->getFd()] = client;
  }
  if (_isInviteOnly) {
    _invited.erase(client->getFd());
  }
}

void Channel::removeClient(int clientFd) {
  const ClientList::iterator it = _clients.find(clientFd);
  if (it != _clients.end()) {
    _clients.erase(it);
  }
  removeOperator(clientFd);
  // broadcast to channel that client has left
}

void Channel::addOperator(Client *client) {
  if (client == NULL) {
    return;
  }
  _operators[client->getFd()] = client;
}

void Channel::removeOperator(int clientFd) {
  if (findClient(_operators, clientFd) != NULL) {
    _operators.erase(clientFd);
  }
  if (_operators.empty() && !_clients.empty()) {
    addOperator(_clients.begin()->second);
    // Not sure this is the right user to promote to operator
  }
}

bool Channel::isValidName(const std::string &name) {
  if (name.empty() || name[0] != '#') {
    return false;
  }
  // TODO: more checks
  return true;
}
