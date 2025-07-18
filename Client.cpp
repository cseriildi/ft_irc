#include "Client.hpp"

#include <sys/socket.h>
#include <sys/types.h>

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "Channel.hpp"
#include "Server.hpp"
#include "utils.hpp"

// * Static members initialization *

const std::map<std::string, CommandFunction> Client::COMMANDS =
    Client::init_commands_map();

std::map<std::string, CommandFunction> Client::init_commands_map() {
  std::map<std::string, CommandFunction> commands;
  commands["PASS"] = &Client::pass;
  commands["NICK"] = &Client::nick;
  commands["USER"] = &Client::user;
  commands["JOIN"] = &Client::join;
  commands["PART"] = &Client::part;
  commands["KICK"] = &Client::kick;
  commands["INVITE"] = &Client::invite;
  commands["TOPIC"] = &Client::topic;
  commands["MODE"] = &Client::mode;
  commands["LIST"] = &Client::list;
  commands["NAMES"] = &Client::names;
  commands["CAP"] = &Client::cap;
  commands["PING"] = &Client::ping;
  commands["QUIT"] = &Client::quit;
  commands["WHOIS"] = &Client::whois;
  commands["PRIVMSG"] = &Client::privmsg;
  commands["TIME"] = &Client::server_time;
  return commands;
}

// * Constructors and destructors *

Client::Client(int sockfd, Server *server)
    : _clientFd(sockfd),
      _joinedAt(0),
      _isPassSet(false),
      _isNickSet(false),
      _isUserSet(false),
      _isAuthenticated(false),
      _wantsToQuit(false),
      _server(server) {}

Client::~Client() {}

// * Getters and setters *

const std::string &Client::getNick() const { return _nick; }
const std::string &Client::getUser() const { return _user; }
const std::string &Client::getHostname() const { return _hostname; }
const std::string &Client::getRealName() const { return _realName; }
const std::string &Client::getPassword() const { return _password; }
time_t Client::getJoinedAt() const { return _joinedAt; }
bool Client::isPassSet() const { return _isPassSet; }
bool Client::isNickSet() const { return _isNickSet; }
bool Client::isUserSet() const { return _isUserSet; }
bool Client::isAuthenticated() const { return _isAuthenticated; }
bool Client::wantsToQuit() const { return _wantsToQuit; }
bool Client::wantsToWrite() const { return !_outBuffer.empty(); }
int Client::getClientFd() const { return _clientFd; }
const ChannelList &Client::getChannels() const { return _channels; }

// * COMMANDS *

void Client::handle(const std::string &msg) {
#ifdef DEBUG
  std::cout << "< Received: " << msg << '\n';
#endif
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
  if (_isPassSet) {
    createMessage(Server::ERR_ALREADYREGISTRED);
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

void Client::leaveAllChannels() {
  for (ChannelList::iterator it = _channels.begin(); it != _channels.end();
       ++it) {
    Channel *channel = it->second;
    channel->removeClient(_clientFd);
    if (channel->getClients().empty()) {
      _server->removeChannel(channel->getName());
    }
  }
}

void Client::broadcastToAllChannels(const std::string &msg,
                                    const std::string &command) {
  std::string reply =
      ":" + _nick + "!~" + _user + "@" + _hostname + " " + command;
  if (command == "PART") {
    for (ChannelList::const_iterator it = _channels.begin();
         it != _channels.end(); ++it) {
      _server->sendToChannel(it->second, reply + " " + it->first + " :" + msg,
                             this);
    }
    return;
  }
  reply += " :" + msg;
  if (_channels.empty()) {
    _server->sendToClient(this, reply );
    return;
  }
  std::vector<int> fds;
  for (ChannelList::const_iterator it = _channels.begin();
       it != _channels.end(); ++it) {
    ClientList cl = it->second->getClients();
    for (ClientList::const_iterator cit = cl.begin(); cit != cl.end(); ++cit) {
      fds.push_back(cit->first);
    }
  }
  const ClientList clients = _server->getClients();
  const std::set<int> uniqueClients(fds.begin(), fds.end());
  for (std::set<int>::const_iterator it = uniqueClients.begin();
       it != uniqueClients.end(); ++it) {
    _server->sendToClient(findClient(clients, *it), reply);
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

void Client::_messageClient(const std::vector<std::string> &msg) {
  const std::string &target = msg[1];
  const std::string &text = msg[2];
  Client *targetClient = findClient(_server->getClients(), target);
  if (targetClient == NULL) {
    createMessage(Server::ERR_NOSUCHNICK, target);
    return;
  }
  const std::string toSend = ":" + _nick + "!~" + _user + "@" + _hostname +
                             " PRIVMSG " + target + " :" + text;
  _server->sendToClient(targetClient, toSend);
}

void Client::_messageChannel(const std::vector<std::string> &msg) {
  const std::string &target = msg[1];
  const std::string &text = msg[2];
  Channel *targetChannel = findChannel(_server->getChannels(), target);
  if (targetChannel == NULL) {
    createMessage(Server::ERR_NOSUCHCHANNEL, target);
    return;
  }
  if (findChannel(_channels, target) == NULL) {
    createMessage(Server::ERR_CANNOTSENDTOCHAN, target);
    return;
  }
  const std::string toSend = ":" + _nick + "!~" + _user + "@" + _hostname +
                             " PRIVMSG " + target + " :" + text;
  _server->sendToChannel(targetChannel, toSend, this);
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
      if (index >= keys.size()) {
        createMessage(Server::ERR_BADCHANNELKEY, name);
        continue;
      }
      const std::string &pass = keys[index];
      if (pass != targetChannel->getPassword()) {
        createMessage(Server::ERR_BADCHANNELKEY, name);
        continue;
      }
    }
    targetChannel->addClient(this);
    _channels[name] = targetChannel;

    _server->sendToChannel(targetChannel, ":" + _nick + "!~" + _user + "@" +
                                              _hostname + " JOIN " + name);
    if (targetChannel->isTopicSet()) {
      createMessage(Server::RPL_TOPIC, targetChannel);
    }

    createMessage(Server::RPL_NAMREPLY, targetChannel);
    createMessage(Server::RPL_ENDOFNAMES, targetChannel);
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
                                        _hostname + " PART " + name + " :" +
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
      createMessage(Server::ERR_USERNOTINCHANNEL, nick + " " + channelName);
      continue;
    }
    _server->sendToChannel(channel, ":" + _nick + "!~" + _user + "@" +
                                        _hostname + " KICK " + channelName +
                                        " " + nick + " :" + reason);
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
  if (msg.size() < 2 || msg[1].empty()) {
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  const std::string &target = msg[1];

  Client *targetClient = findClient(_server->getClients(), target);
  if (targetClient != NULL) {
    // We don't support user MODE
    return;
  }

  Channel *channel = findChannel(_server->getChannels(), target);
  if (channel == NULL) {
    createMessage(Server::ERR_NOSUCHCHANNEL, target);
    return;
  }
  if (msg.size() < 3 || msg[2].empty()) {
    createMessage(Server::RPL_CHANNELMODEIS, channel);
    return;
  }
  const std::string &modes = msg[2];
  std::vector<std::string> params(msg.begin() + 3, msg.end());

  const size_t c = modes.find_first_not_of("+-itklo");
  if (c != std::string::npos) {
    createMessage(Server::ERR_UNKNOWNMODE, modes.substr(c, 1), target);
    return;
  }
  if (modes.find_first_of("itklo") == std::string::npos) {
    return;
  }
  if (findClient(channel->getOperators(), _clientFd) == NULL) {
    createMessage(Server::ERR_CHANOPRIVSNEEDED, target);
    return;
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
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  setting = true;
  std::vector<std::pair<char, char> > changes;
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
        createMessage(Server::ERR_USERNOTINCHANNEL, nick + " " + target);
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
  std::vector<std::pair<char, char> >::const_iterator it = changes.begin();
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

// * HELPERS *

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

// * COMMUNICATION *

void Client::receive() {
  char buffer[BUFFER_SIZE];
  memset(buffer, 0, sizeof(buffer));  // NOLINT

  const ssize_t received =
      recv(_clientFd, buffer, sizeof(buffer) - 1, 0);  // NOLINT
  if (received == 0) {
    throw std::runtime_error("Client disconnected");
  }
  if (received == -1) {
    throw std::runtime_error("Error receiving data: " +
                             std::string(strerror(errno)));
  }
  _inBuffer.append(buffer, received);  // NOLINT

  size_t pos = 0;
  while ((pos = _inBuffer.find("\r\n")) != std::string::npos) {
    std::string const line = _inBuffer.substr(0, pos);
    handle(line);
    _inBuffer.erase(0, pos + 2);
  }
}

void Client::answer() {
#ifdef DEBUG
  std::cout << "> Answered: " << _outBuffer << '\n';
#endif

  while (!_outBuffer.empty()) {
    const ssize_t sent =
        send(_clientFd, _outBuffer.c_str(), _outBuffer.length(), 0);  // NOLINT
    if (sent == -1) {
      throw std::runtime_error("Error sending data: " +
                               std::string(strerror(errno)));
    }
    _outBuffer.erase(0, sent);
  }
}

// * MESSAGES *

void Client::createMessage(ERR error_code, const std::string &param,
                           const std::string &end) {
  std::stringstream ss;
  ss << ":" << _server->getName() << " " << error_code;
  ss << " " << (_isAuthenticated ? _nick : "*") << " ";
  ss << param << (param.empty() ? "" : " ") << ":";

  if (Server::ERRORS.find(error_code) != Server::ERRORS.end()) {
    ss << Server::ERRORS.at(error_code);
    if (!end.empty()) {
      ss << " " << end;
    }
  } else {
    ss << "Unknown error";
  }
  _server->sendToClient(this, ss.str());
}

void Client::createMessage(RPL response_code) {
  std::stringstream ss;
  ss << ":" << _server->getName() << " " << std::setw(3) << std::setfill('0')
     << response_code << " " << _nick << " ";
  if (response_code == Server::RPL_WELCOME) {
    ss << ":Welcome to the Internet Relay Network " << _nick << "!~" << _user
       << "@" << _hostname;
  } else if (response_code == Server::RPL_YOURHOST) {
    ss << ":Your host is " << _server->getName() << ", running version 1.0";
  } else if (response_code == Server::RPL_CREATED) {
    ss << ":This server was created " << get_time(_server->getCreatedAt());
  } else if (response_code == Server::RPL_MYINFO) {
    ss << _server->getName() << " 1.0 "
       << "- " << "itklo";
  } else if (response_code == Server::RPL_LISTEND) {
    ss << ":End of LIST";
  } else if (response_code == Server::RPL_TIME) {
    ss << _server->getName() << " :" << get_time(std::time(NULL));
  } else if (response_code == Server::RPL_ENDOFWHOIS) {
    ss << ":End of WHOIS list";
  } else if (response_code == Server::RPL_ENDOFNAMES) {
    ss << ":End of NAMES list";
  } else {
    ss << ":Unknown response code";
  }
  _server->sendToClient(this, ss.str());
}

void Client::createMessage(RPL response_code, Client *targetClient) {
  std::stringstream ss;
  ss << ":" << _server->getName() << " " << response_code << " " << _nick << " "
     << targetClient->getNick() << " ";
  if (response_code == Server::RPL_WHOISUSER) {
    ss << "~" << targetClient->getUser() << " " << targetClient->getHostname()
       << " * :" << targetClient->getRealName();
  } else if (response_code == Server::RPL_WHOISCHANNELS) {
    ss << ":";
    const ChannelList &channels = targetClient->getChannels();
    for (ChannelList::const_iterator it = channels.begin();
         it != channels.end(); ++it) {
      if (findClient(it->second->getClients(), targetClient->_clientFd) !=
          NULL) {
        ss << "@";  // Channel operator
      }
      ss << it->first;
      ChannelList::const_iterator nextIt = it;
      ++nextIt;
      if (nextIt != channels.end()) {
        ss << " ";
      }
    }
  } else if (response_code == Server::RPL_WHOISSERVER) {
    ss << _server->getName() << " :ft_irc server";
  } else if (response_code == Server::RPL_WHOISIDLE) {
    ss << (time(NULL) - targetClient->getJoinedAt()) << " :seconds idle";
  } else {
    ss << ":Unknown response code";
  }
  _server->sendToClient(this, ss.str());
}

void Client::createMessage(RPL response_code, Channel *targetChannel) {
  if (targetChannel == NULL) {
    return;
  }
  std::stringstream ss;
  ss << ":" << _server->getName() << " " << response_code << " " << _nick
     << " ";

  if (response_code == Server::RPL_LIST) {
    ss << targetChannel->getName() << " " << targetChannel->getClients().size()
       << " :" << targetChannel->getTopic();
  } else if (response_code == Server::RPL_CHANNELMODEIS) {
    ss << targetChannel->getName() << " " << targetChannel->getMode(this);
  } else if (response_code == Server::RPL_NOTOPIC) {
    ss << targetChannel->getName() << " :No topic is set";
  } else if (response_code == Server::RPL_TOPIC) {
    ss << targetChannel->getName() << " :" << targetChannel->getTopic();
  } else if (response_code == Server::RPL_NAMREPLY) {
    const ClientList &clients = targetChannel->getClients();
    const ClientList &operators = targetChannel->getOperators();
    ss << "= " << targetChannel->getName() << " :";
    for (ClientList::const_iterator it = clients.begin(); it != clients.end();
         ++it) {
      if (it != clients.begin()) {
        ss << " ";
      }
      if (findClient(operators, it->first) != NULL) {
        ss << "@";  // Channel operator
      }
      ss << it->second->getNick();
    }
  } else if (response_code == Server::RPL_LISTEND) {
    ss << targetChannel->getName() << " :End of LIST";
  } else if (response_code == Server::RPL_ENDOFNAMES) {
    ss << targetChannel->getName() << " :End of NAMES list";
  } else {
    ss << targetChannel->getName() << " :Unknown response code";
  }
  _server->sendToClient(this, ss.str());
}

void Client::createMessage(RPL response_code, Channel *targetChannel,
                           Client *targetClient) {
  if (targetChannel == NULL || targetClient == NULL) {
    return;
  }
  std::stringstream ss;
  ss << ":" << _server->getName() << " " << response_code << " " << _nick << " "
     << targetChannel->getName() << " ";
  if (response_code == Server::RPL_INVITING) {
    ss << targetClient->getNick();
  } else {
    ss << ":Unknown response code";
  }
  _server->sendToClient(this, ss.str());
}
