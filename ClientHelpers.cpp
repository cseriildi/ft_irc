#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <string>
#include <utility>
#include <vector>

#include "Channel.hpp"
#include "Client.hpp"
#include "utils.hpp"

bool Client::isValidName(const std::string &name) {
  if (name.empty() || name.length() > 50 || std::isalpha(name[0]) == 0 ||
      name.find_first_of(" ,:") != std::string::npos) {
    return false;
  }
  for (std::string::const_iterator it = name.begin(); it != name.end(); ++it) {
    if (std::isprint(*it) == 0) {
      return false;
    }
  }
  return true;
}

void Client::removeChannel(const std::string &name) {
  Channel *channel = findChannel(_channels, name);
  if (channel != NULL) {
    _channels.erase(name);
    channel->removeClient(_clientFd);
    if (channel->getClients().empty()) {
      _server->removeChannel(channel->getName());
    }
  }
}

void Client::_authenticate() {
  if (_server->isPassRequired() &&
      (!_isPassSet || _server->getPassword() != _password)) {
    createMessage(Server::ERR_PASSWDMISMATCH);
    return;
  }
  _joinedAt = time(NULL);
  _isAuthenticated = true;
  createMessage(Server::RPL_WELCOME);
  createMessage(Server::RPL_YOURHOST);
  createMessage(Server::RPL_CREATED);
  createMessage(Server::RPL_MYINFO);
}

void Client::appendToOutBuffer(const std::string &msg) {
  _outBuffer += msg + "\r\n";
}

void Client::joinChannel(Channel *channel) {
  channel->addClient(this);
  const std::string &name = channel->getName();
  _channels[name] = channel;

  _server->sendToChannel(
      channel, ":" + _nick + "!~" + _user + "@" + _hostname + " JOIN " + name);
  if (channel->isTopicSet()) {
    createMessage(Server::RPL_TOPIC, channel);
  }

  createMessage(Server::RPL_NAMREPLY, channel);
  createMessage(Server::RPL_ENDOFNAMES, channel);
}

void Client::leaveAllChannels() {
  for (ChannelList::iterator it = _channels.begin(); it != _channels.end();
       ++it) {
    Channel *channel = it->second;
    channel->removeClient(_clientFd);
    if (channel->getClients().empty()) {
      _server->removeChannel(channel->getName());
    }
  }
  _channels.clear();
}

bool Client::modeCheck(const std::string &modes, Channel *channel,
                       std::vector<std::string> &params) {
  const std::string name = channel->getName();
  const size_t c = modes.find_first_not_of("+-itklo");
  if (c != std::string::npos) {
    createMessage(Server::ERR_UNKNOWNMODE, modes.substr(c, 1), name);
    return false;
  }
  if (modes.find_first_of("itklo") == std::string::npos) {
    return false;
  }
  if (findClient(channel->getOperators(), _clientFd) == NULL) {
    createMessage(Server::ERR_CHANOPRIVSNEEDED, name);
    return false;
  }

  bool setting = true;
  size_t parameter_count = 0;
  for (std::string::const_iterator it = modes.begin(); it != modes.end();
       ++it) {
    if (*it == '+' || *it == '-') {
      setting = (*it == '+');
      continue;
    }
    if (*it == 'k' || *it == 'o' || (setting && *it == 'l')) {
      parameter_count++;
    }
  }
  if (params.size() < parameter_count) {
    createMessage(Server::ERR_NEEDMOREPARAMS, "MODE");
    return false;
  }
  return true;
}

Channel *Client::findChannelForMode(const std::vector<std::string> &msg) {
  if (msg.size() < 2 || msg[1].empty()) {
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return NULL;
  }
  const std::string &target = msg[1];

  Client *targetClient = findClient(_server->getClients(), target);
  if (targetClient != NULL) {
    return NULL;  // We don't support user MODE
  }

  Channel *channel = findChannel(_server->getChannels(), target);
  if (channel == NULL) {
    createMessage(Server::ERR_NOSUCHCHANNEL, target);
    return NULL;
  }
  if (msg.size() < 3 || msg[2].empty()) {
    createMessage(Server::RPL_CHANNELMODEIS, channel);
    return NULL;
  }
  return channel;
}

ModeChanges Client::changeMode(std::vector<std::string> &params,
                               Channel *channel, const std::string &modes) {
  bool setting = true;
  ModeChanges changes;
  std::vector<std::string>::iterator param_it = params.begin();
  for (std::string::const_iterator it = modes.begin(); it != modes.end();
       ++it) {
    if (*it == '+' || *it == '-') {
      setting = (*it == '+');
      continue;
    }
    if (*it == 'i') {
      if (setting == channel->isInviteOnly()) {
        continue;
      }
      channel->setInviteOnly(setting);
    } else if (*it == 't') {
      if (setting == channel->isTopicOperOnly()) {
        continue;
      }
      channel->setTopicOperOnly(setting);
    } else if (*it == 'k') {
      if (setting) {
        channel->setPass(*param_it++);
      } else if (channel->getPassword() != *param_it++) {
        continue;
      } else {
        channel->setPassRequired(false);
        channel->setPass("");
      }
    } else if (*it == 'l') {
      if (setting) {
        const int limit = std::atoi((*param_it).c_str());
        if (limit < 0) {
          param_it = params.erase(param_it);
          continue;
        }
        ++param_it;
        channel->setLimit(limit);
      } else if (!channel->isLimited()) {
        continue;
      } else {
        channel->setLimited(false);
      }
    } else if (*it == 'o') {
      const std::string &nick = *param_it;
      Client *targetClient = findClient(channel->getClients(), nick);
      if (targetClient == NULL) {
        createMessage(Server::ERR_USERNOTINCHANNEL,
                      nick + " " + channel->getName());
        param_it = params.erase(param_it);
        continue;
      }
      ++param_it;
      if (setting) {
        channel->addOperator(targetClient);
      } else {
        channel->removeOperator(targetClient->getClientFd());
      }
    }
    changes.push_back(std::make_pair(*it, (setting ? '+' : '-')));
  }
  while (param_it != params.end()) {
    param_it = params.erase(param_it);
  }
  return changes;
}
