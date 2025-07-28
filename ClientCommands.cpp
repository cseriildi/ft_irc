#include <cctype>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <iterator>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Client.hpp"
#include "Server.hpp"
#include "utils.hpp"

void Client::handle(const std::string &msg) {
  std::vector<std::string> parsed = parse(msg);

  if (parsed.empty()) {
    return;  // Ignore empty lines
  }
  if (!_isAuthenticated && parsed[0] != "PASS" && parsed[0] != "NICK" &&
      parsed[0] != "USER" && parsed[0] != "CAP") {
    createMessage(Server::ERR_NOTREGISTERED);
    return;
  }
  const std::map<std::string, CommandFunction>::const_iterator fn =
      COMMANDS.find(parsed[0]);
  if (fn == COMMANDS.end()) {
    createMessage(Server::ERR_UNKNOWNCOMMAND, parsed[0]);
    return;
  }
  const CommandFunction command = fn->second;
  (this->*command)(parsed);
}

void Client::pass(const std::vector<std::string> &msg) {
  if (_isPassSet || _isNickSet || _isUserSet) {
    createMessage(Server::ERR_ALREADYREGISTRED);
    _wantsToQuit = true;
    return;
  }
  if (msg.size() < 2) {
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  _password = msg[1];
  _isPassSet = true;
}

void Client::nick(const std::vector<std::string> &msg) {
  if (msg.size() < 2 || msg[1].empty()) {
    createMessage(Server::ERR_NONICKNAMEGIVEN, msg[0]);
    return;
  }
  const std::string &nick = msg[1];
  if (!Client::isValidName(nick)) {
    createMessage(Server::ERR_ERRONEUSNICKNAME, nick);
    return;
  }
  if (!_server->isNicknameAvailable(this, nick)) {
    createMessage(Server::ERR_NICKNAMEINUSE, nick);
    return;
  }
  if (_isAuthenticated) {
    broadcastToAllChannels(nick, "NICK");
  } else if (_isNickSet) {
    createMessage(Server::ERR_ALREADYREGISTRED);
    return;
  }
  _nick = nick;
  _isNickSet = true;
  if (!_isAuthenticated && _isUserSet) {
    _authenticate();
  }
}

void Client::user(const std::vector<std::string> &msg) {
  if (_isUserSet) {
    createMessage(Server::ERR_ALREADYREGISTRED);
    return;
  }
  if (msg.size() < 5) {  // NOLINT
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }

  _user = msg[1];
  // mode is usually ignored in irc servers
  _hostname = msg[3];
  _realName = msg[4];
  _isUserSet = true;
  if (_isNickSet) {
    _authenticate();
  }
}

void Client::cap(const std::vector<std::string> &msg) {
  if (msg.size() >= 2 && msg[1] == "LS") {
    _server->sendToClient(this, ":" + _server->getName() + " CAP " +
                                    (_isAuthenticated ? _nick : "*") + " LS :");
  }
}

void Client::ping(const std::vector<std::string> &msg) {
  if (msg.size() < 2 || msg[1].empty()) {
    createMessage(Server::ERR_NOORIGIN);
    return;
  }
  if (msg.size() == 2) {
    _server->sendToClient(this, "PONG :" + msg[1]);
  } else if (msg[2] == _server->getName()) {
    _server->sendToClient(this, "PONG " + msg[2] + " " + msg[1]);
  } else {
    createMessage(Server::ERR_NOSUCHSERVER, msg[2]);
  }
}

void Client::quit(const std::vector<std::string> &msg) {
  const std::string reason = (msg.size() > 1 ? msg[1] : "Client Quit");

  broadcastToAllChannels(reason, "QUIT");
  leaveAllChannels();
  _wantsToQuit = true;
}

void Client::whois(const std::vector<std::string> &msg) {
  if (msg.size() < 2 || msg[1].empty()) {
    createMessage(Server::ERR_NONICKNAMEGIVEN);
    return;
  }
  const std::string &target = msg[1];

  Client *targetClient = findClient(_server->getClients(), target);
  if (targetClient == NULL) {
    createMessage(Server::ERR_NOSUCHNICK, target);
    return;
  }
  createMessage(Server::RPL_WHOISUSER, targetClient);
  if (!_channels.empty()) {
    createMessage(Server::RPL_WHOISCHANNELS, targetClient);
  }
  createMessage(Server::RPL_WHOISSERVER, targetClient);
  createMessage(Server::RPL_WHOISIDLE, targetClient);
  createMessage(Server::RPL_ENDOFWHOIS);
}

void Client::privmsg(const std::vector<std::string> &msg) {
  if (msg.size() < 2) {
    createMessage(Server::ERR_NORECIPIENT, "", "(" + msg[0] + ")");
    return;
  }
  if (msg.size() < 3) {
    createMessage(Server::ERR_NOTEXTTOSEND, msg[0]);
    return;
  }
  if (!msg[1].empty() &&
      std::string(CHANNEL_PREFIXES).find(msg[1][0]) != std::string::npos) {
    _messageChannel(msg);
  } else {
    _messageClient(msg);
  }
}

void Client::join(const std::vector<std::string> &msg) {
  if (msg.size() < 2 || msg[1].empty()) {
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  const std::vector<std::string> channels = split(msg[1], ',');
  const std::vector<std::string> keys =
      split((msg.size() > 2 ? msg[2] : ""), ',');
  if (*channels.begin() == "0") {
    broadcastToAllChannels("", "PART");
    leaveAllChannels();
    return;
  }
  for (std::vector<std::string>::const_iterator it = channels.begin();
       it != channels.end(); ++it) {
    const std::string &name = *it;
    if (!Channel::isValidName(name)) {
      createMessage(Server::ERR_NOSUCHCHANNEL, name);
      continue;
    }
    Channel *targetChannel = findChannel(_server->getChannels(), name);
    if (targetChannel == NULL) {
      targetChannel = new Channel(name, _server);
      _server->addChannel(targetChannel);
    } else if (findChannel(_channels, name) != NULL) {
      continue;  // Already in the channel
    }
    if (targetChannel->isInviteOnly() &&
        findClient(targetChannel->getInvited(), _clientFd) == NULL) {
      createMessage(Server::ERR_INVITEONLYCHAN, name);
      continue;
    }
    if (targetChannel->isLimited() &&
        targetChannel->getClients().size() >= targetChannel->getLimit()) {
      createMessage(Server::ERR_CHANNELISFULL, name);
      continue;
    }
    if (targetChannel->isPassRequired()) {
      size_t const index = std::distance(channels.begin(), it);
      if (index >= keys.size() || keys[index] != targetChannel->getPassword()) {
        createMessage(Server::ERR_BADCHANNELKEY, name);
        continue;
      }
    }
    joinChannel(targetChannel);
  }
}

void Client::part(const std::vector<std::string> &msg) {
  if (msg.size() < 2 || msg[1].empty()) {
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  const std::vector<std::string> channels = split(msg[1], ',');
  for (std::vector<std::string>::const_iterator it = channels.begin();
       it != channels.end(); ++it) {
    const std::string &name = *it;
    Channel *channel = findChannel(_server->getChannels(), name);
    if (channel == NULL) {
      createMessage(Server::ERR_NOSUCHCHANNEL, name);
      continue;
    }
    if (findChannel(_channels, name) == NULL) {
      createMessage(Server::ERR_NOTONCHANNEL, name);
      continue;
    }
    const std::string reason = (msg.size() > 2 ? msg[2] : "");
    _server->sendToChannel(channel, ":" + _nick + "!~" + _user + "@" +
                                        _hostname + " PART " + name +
                                        " :" +  // NOLINT
                                        reason);
    removeChannel(name);
  }
}

void Client::kick(const std::vector<std::string> &msg) {
  if (msg.size() < 3 || msg[1].empty() || msg[2].empty()) {
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  const std::vector<std::string> channels = split(msg[1], ',');
  const std::vector<std::string> clients = split(msg[2], ',');
  if (channels.size() != clients.size() && channels.size() != 1) {
    /*  For the message to be syntactically correct, there MUST be
    either one channel parameter and multiple user parameter, or as many
    channel parameters as there are user parameters. */
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  const std::string reason = (msg.size() > 3 ? msg[3] : _nick);
  std::vector<std::string>::const_iterator channelIt = channels.begin();
  std::vector<std::string>::const_iterator clientIt = clients.begin();

  for (; clientIt != clients.end(); ++clientIt) {
    const std::string &channelName = *channelIt;
    const std::string &nick = *clientIt;
    if (channels.size() != 1) {
      ++channelIt;
    }

    Channel *channel = findChannel(_server->getChannels(), channelName);
    if (channel == NULL) {
      createMessage(Server::ERR_NOSUCHCHANNEL, channelName);
      continue;
    }
    if (findChannel(_channels, channelName) == NULL) {
      createMessage(Server::ERR_NOTONCHANNEL, channelName);
      continue;
    }
    if (findClient(channel->getOperators(), _clientFd) == NULL) {
      createMessage(Server::ERR_CHANOPRIVSNEEDED, channelName);
      continue;
    }
    Client *targetClient = findClient(channel->getClients(), nick);
    if (targetClient == NULL) {
      createMessage(Server::ERR_USERNOTINCHANNEL,
                    nick + " " + channelName);  // NOLINT
      continue;
    }
    _server->sendToChannel(channel, ":" + _nick + "!~" + _user + "@" +
                                        _hostname + " KICK " + channelName +
                                        " " + nick + " :" + reason);  // NOLINT
    targetClient->removeChannel(channelName);
  }
}

void Client::invite(const std::vector<std::string> &msg) {
  if (msg.size() < 3 || msg[1].empty() || msg[2].empty()) {
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  const std::string &nick = msg[1];
  const std::string &channel = msg[2];

  Client *targetClient = findClient(_server->getClients(), nick);
  if (targetClient == NULL) {
    createMessage(Server::ERR_NOSUCHNICK, nick);
    return;
  }
  Channel *targetChannel = findChannel(_server->getChannels(), channel);
  if (targetChannel == NULL) {
    createMessage(Server::ERR_NOSUCHCHANNEL, channel);
    return;
  }
  const ClientList clients = targetChannel->getClients();
  if (findClient(clients, targetClient->getClientFd()) != NULL) {
    createMessage(Server::ERR_USERONCHANNEL, nick + " " + channel);
    return;
  }
  if (findClient(clients, _clientFd) == NULL) {
    createMessage(Server::ERR_NOTONCHANNEL, channel);
    return;
  }
  if (targetChannel->isInviteOnly() &&
      findClient(targetChannel->getOperators(), _clientFd) == NULL) {
    createMessage(Server::ERR_CHANOPRIVSNEEDED, channel);
    return;
  }
  targetChannel->addInvited(targetClient);
  _server->sendToClient(targetClient, ":" + _nick + "!~" + _user + "@" +
                                          _hostname + " INVITE " + nick + " " +
                                          channel);
  createMessage(Server::RPL_INVITING, targetChannel, targetClient);
}

void Client::topic(const std::vector<std::string> &msg) {
  if (msg.size() < 2 || msg[1].empty()) {
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  const std::string &target = msg[1];
  Channel *channel = findChannel(_server->getChannels(), target);
  if (channel == NULL) {
    createMessage(Server::ERR_NOSUCHCHANNEL, target);
    return;
  }
  if (findChannel(_channels, target) == NULL) {
    createMessage(Server::ERR_NOTONCHANNEL, target);
    return;
  }
  if (msg.size() > 2) {
    if (channel->isTopicOperOnly() &&
        findClient(channel->getOperators(), _clientFd) == NULL) {
      createMessage(Server::ERR_CHANOPRIVSNEEDED, target);
      return;
    }
    channel->setTopic(msg[2]);
    channel->setTopicSet(true);
    _server->sendToChannel(channel, ":" + _nick + "!~" + _user + "@" +
                                        _hostname + " TOPIC " + target + " :" +
                                        channel->getTopic());
  }
  if (!channel->isTopicSet()) {
    createMessage(Server::RPL_NOTOPIC, channel);
  } else {
    createMessage(Server::RPL_TOPIC, channel);
  }
}
void Client::mode(const std::vector<std::string> &msg) {
  Channel *channel = findChannelForMode(msg);
  if (channel == NULL) {
    return;
  }
  const std::string &modes = msg[2];
  std::vector<std::string> params(msg.begin() + 3, msg.end());

  if (!modeCheck(modes, channel, params)) {
    return;
  }
  ModeChanges changes = changeMode(params, channel, modes);
  if (changes.empty()) {
    return;
  }
  ModeChanges::const_iterator it = changes.begin();
  std::string mode_change;
  for (; it != changes.end(); ++it) {
    if (mode_change.empty() || (it - 1)->second != it->second) {
      mode_change += it->second;
    }
    mode_change += it->first;
  }
  for (std::vector<std::string>::const_iterator it = params.begin();
       it != params.end(); ++it) {
    mode_change += " " + *it;
  }
  if (!mode_change.empty()) {
    _server->sendToChannel(channel, ":" + _nick + "!~" + _user + "@" +
                                        _hostname + " MODE " +
                                        channel->getName() + " " + mode_change);
  }
}

void Client::names(const std::vector<std::string> &msg) {
  if (msg.size() > 2 && msg[2] != _server->getName()) {
    createMessage(Server::ERR_NOSUCHSERVER, msg[2]);
    return;
  }
  if (msg.size() == 1) {
    for (ChannelList::const_iterator it = _server->getChannels().begin();
         it != _server->getChannels().end(); ++it) {
      createMessage(Server::RPL_NAMREPLY, it->second);
    }
  } else {
    std::vector<std::string> channels = split(msg[1], ',');
    for (std::vector<std::string>::const_iterator it = channels.begin();
         it != channels.end(); ++it) {
      Channel *channel = findChannel(_server->getChannels(), *it);
      if (channel != NULL) {
        createMessage(Server::RPL_NAMREPLY, channel);
      }
    }
  }
  createMessage(Server::RPL_ENDOFNAMES);
}

void Client::list(const std::vector<std::string> &msg) {
  if (msg.size() > 2 && msg[2] != _server->getName()) {
    createMessage(Server::ERR_NOSUCHSERVER, msg[2]);
    return;
  }
  if (msg.size() == 1) {
    for (ChannelList::const_iterator it = _server->getChannels().begin();
         it != _server->getChannels().end(); ++it) {
      createMessage(Server::RPL_LIST, it->second);
    }
  } else {
    std::vector<std::string> channels = split(msg[1], ',');
    for (std::vector<std::string>::const_iterator it = channels.begin();
         it != channels.end(); ++it) {
      Channel *channel = findChannel(_server->getChannels(), *it);
      if (channel != NULL) {
        createMessage(Server::RPL_LIST, channel);
      }
    }
  }
  createMessage(Server::RPL_LISTEND);
}

void Client::server_time(const std::vector<std::string> &msg) {
  if (msg.size() > 1 && msg[1] != _server->getName()) {
    createMessage(Server::ERR_NOSUCHSERVER, msg[1]);
    return;
  }
  createMessage(Server::RPL_TIME);
}
