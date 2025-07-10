#include "Channel.hpp"

#include <cstddef>
#include <map>
#include <sstream>
#include <string>

#include "Client.hpp"
#include "Server.hpp"
#include "utils.hpp"

Channel::Channel(const std::string &name, Server *server)
    : _name(name),
      _isInviteOnly(false),
      _topicOperOnly(false),
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
bool Channel::isTopicOperOnly() const { return _topicOperOnly; }
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
void Channel::setTopicOperOnly(bool topicOperOnly) {
  _topicOperOnly = topicOperOnly;
}

std::string Channel::getMode(Client *client) const {
  std::string mode;
  if (isInviteOnly()) {
    mode += "i";
  }
  if (isTopicOperOnly()) {
    mode += "t";
  }
  if (isPassRequired()) {
    mode += "k";
  }
  if (isLimited()) {
    mode += "l";
  }
  if (!mode.empty()) {
    std::stringstream ss;
    ss << "+" << mode;
    if (findClient(_clients, client->getClientFd()) != NULL) {
      if (isPassRequired()) {
        ss << " " << _password;
      }
      if (isLimited()) {
        ss << " " << _limit;
      }
    }
    mode = ss.str();
  }
  return mode;
}

void Channel::addClient(Client *client) {
  if (client == NULL) {
    return;
  }
  if (_clients.empty()) {
    _operators[client->getClientFd()] = client;
  }
  _clients[client->getClientFd()] = client;
  if (_isInviteOnly) {
    _invited.erase(client->getClientFd());
  }
}

void Channel::removeClient(int clientFd) {
  const ClientList::iterator it = _clients.find(clientFd);
  if (it != _clients.end()) {
    _clients.erase(it);
  }
  removeOperator(clientFd);
}

void Channel::addOperator(Client *client) {
  if (client == NULL) {
    return;
  }
  _operators[client->getClientFd()] = client;
}

void Channel::removeOperator(int clientFd) {
  if (findClient(_operators, clientFd) != NULL) {
    _operators.erase(clientFd);
  }
}

void Channel::addInvited(Client *client) {
  if (client == NULL) {
    return;
  }
  _invited[client->getClientFd()] = client;
}

bool Channel::isValidName(const std::string &name) {
  return !name.empty() && name[0] == '#' && name.length() <= 50 &&
         name.find_first_of(" ,:") == std::string::npos &&
         name.find('\a') == std::string::npos;
}
