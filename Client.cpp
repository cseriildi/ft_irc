#include "Client.hpp"

#include <sys/socket.h>
#include <sys/types.h>

#include <cerrno>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Server.hpp"
#include "utils.hpp"

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
  commands["WHO"] = &Client::who;
  commands["WHOIS"] = &Client::whois;
  commands["PRIVMSG"] = &Client::privmsg;
  // TODO: add rest of commands
  return commands;
}

Client::Client(int sockfd, Server *server)
    : _clientFd(sockfd),
      _mode(0),
      _isPassSet(false),
      _isNickSet(false),
      _isUserSet(false),
      _isAuthenticated(false),
      _wantsToQuit(false),
      _server(server) {}

Client::~Client() {}

const std::string &Client::getNick() const { return _nick; }
const std::string &Client::getUser() const { return _user; }
int Client::getMode() const { return _mode; }
const std::string &Client::getHostname() const { return _hostname; }
const std::string &Client::getRealName() const { return _realName; }
const std::string &Client::getPassword() const { return _password; }
bool Client::isPassSet() const { return _isPassSet; }
bool Client::isNickSet() const { return _isNickSet; }
bool Client::isUserSet() const { return _isUserSet; }
bool Client::isAuthenticated() const { return _isAuthenticated; }
bool Client::wantsToQuit() const { return _wantsToQuit; }
int Client::getClientFd() const { return _clientFd; }
const ChannelList &Client::getChannels() const { return _channels; }

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

bool Client::wantsToWrite() const { return !_outBuffer.empty(); }

void Client::handle(const std::string &msg) {
#ifdef DEBUG
  std::cout << "< Received: " << msg << '\n';
#endif

  // TODO: think about leading spaces
  std::vector<std::string> parsed = split(msg);

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

void Client::_authenticate() {
  if (_server->isPassRequired() &&
      (!_isPassSet || _server->getPassword() != _password)) {
    createMessage(Server::ERR_PASSWDMISMATCH);
    return;
  }
  _isAuthenticated = true;
  createMessage(Server::RPL_WELCOME);
  createMessage(Server::RPL_YOURHOST);
  createMessage(Server::RPL_CREATED);
  createMessage(Server::RPL_MYINFO);
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
  std::string password = msg[1];

  if (password[0] == ':')
    password = password.substr(1);  // Maybe I can remove it in the split
  // TODO: think about space in password
  _password = password;
  _isPassSet = true;
}
void Client::_broadcastNickChange(const std::string &newNick) {
  const std::string msg =
      ":" + _nick + "!~" + _user + "@" + _hostname + " NICK :" + newNick;

  // to self
  _server->sendToClient(this, msg);

  // to the user's channels
  for (ChannelList::const_iterator it = _channels.begin();
       it != _channels.end(); ++it) {
    _server->sendToChannel(it->second, msg);
  }
}

void Client::nick(const std::vector<std::string> &msg) {
  if (msg.size() < 2) {
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  std::string nick = msg[1];

  if (nick[0] == ':') nick = nick.substr(1);
  if (nick.empty()) {
    createMessage(Server::ERR_NONICKNAMEGIVEN);
    return;
  }
  if (nick.find(' ') != std::string::npos) {
    createMessage(Server::ERR_ERRONEUSNICKNAME, nick);
    return;
  }
  if (!_server->isNicknameAvailable(this, nick)) {
    createMessage(Server::ERR_NICKNAMEINUSE, nick);
    return;
  }
  if (_isAuthenticated) {
    _broadcastNickChange(nick);
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
  _mode = 0;  // TODO: extract mode
  _hostname = msg[3];

  std::string realname = msg[4];
  if (realname[0] == ':') {
    realname = realname.substr(1);
  }
  _realName = realname;
  _isUserSet = true;
  if (_isNickSet) {
    _authenticate();
  }
}

void Client::cap(const std::vector<std::string> &msg) {
  if (msg.size() >= 2 && msg[1] == "LS") {
    std::string capabilities = ":" + _server->getName() + " CAP " +
                               (_isAuthenticated ? _nick : "*") + " LS :";
    for (std::map<std::string, CommandFunction>::const_iterator it =
             COMMANDS.begin();
         it != COMMANDS.end(); ++it) {
      if (it->first != "CAP") {  // Don't include CAP itself
        capabilities += it->first;
        std::map<std::string, CommandFunction>::const_iterator nextIt = it;
        ++nextIt;
        if (nextIt != COMMANDS.end()) {
          capabilities += " ";  // Add space between capabilities
        }
      }
    }
    _server->sendToClient(this, capabilities);
  }
}

void Client::ping(const std::vector<std::string> &msg) {
  if (msg.size() != 2 || msg[1] == ":") {
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  // TODO: 2 params given
  _server->sendToClient(this, "PONG " + msg[1]);
}

void Client::quit(const std::vector<std::string> &msg) {
  std::string reason = "Client quit";
  if (msg.size() > 1) {
    reason = msg[1];
    if (reason[0] == ':') {
      reason = reason.substr(1);
    }
  }
  for (ChannelList::iterator it = _channels.begin(); it != _channels.end();
       ++it) {
    Channel *channel = it->second;
    channel->removeClient(_clientFd);
    _server->sendToChannel(channel, ":" + _nick + "!~" + _user + "@" +
                                        _hostname + " QUIT :" + reason);
    if (channel->getClients().empty()) {
      _server->removeChannel(channel->getName());
    }
  }
  // maybe we have to send a message to the client before closing the fd
  //_server->removeClient(_clientFd);
  _wantsToQuit = true;
}

void Client::whois(const std::vector<std::string> &msg) {
  if (msg.size() < 2) {
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  std::string target = msg[1];
  if (target[0] == ':') {
    target = target.substr(1);
  }
  Client *targetClient = findClient(_server->getClients(), target);
  if (targetClient == NULL) {
    createMessage(Server::ERR_NOSUCHNICK, target);
    return;
  }
  createMessage(Server::RPL_WHOISUSER, targetClient);
  createMessage(Server::RPL_WHOISCHANNELS, targetClient);
  createMessage(Server::RPL_WHOISIDLE, targetClient);
  createMessage(Server::RPL_WHOISSERVER, targetClient);
  createMessage(Server::RPL_ENDOFWHOIS);
}

void Client::who(const std::vector<std::string> &msg) { (void)msg; }

void Client::privmsg(const std::vector<std::string> &msg) {
  if (msg.size() < 2) {
    createMessage(Server::ERR_NORECIPIENT, msg[0]);
    return;
  }
  if (msg.size() < 3) {
    createMessage(Server::ERR_NOTEXTTOSEND, msg[0]);
    return;
  }
  if ((msg[1].size() > 0 && (msg[1][0] == '#' || msg[1][0] == '&' || msg[1][0] == '+' || msg[1][0] == '!')) ||
      (msg[1].size() > 1 && (msg[1][1] == '#' || msg[1][1] == '&' || msg[1][1] == '+' || msg[1][1] == '!'))) {
    _messageChannel(msg);
  } else {
    _messageClient(msg);
  }
 
}

void Client::_messageClient(const std::vector<std::string> &msg) {
  std::string target = msg[1];
  if (target[0] == ':') {
    target = target.substr(1);
  }
  std::string text = msg[2];
  if (text[0] == ':') {
    text = text.substr(1);
  }
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
  std::string target = msg[1];
  if (target[0] == ':') {
    target = target.substr(1);
  }
  std::string text = msg[2];
  if (text[0] == ':') {
    text = text.substr(1);
  }
  Channel *targetChannel = findChannel(_server->getChannels(), target);
  if (targetChannel == NULL) {
    createMessage(Server::ERR_NOSUCHCHANNEL, target);
    return;
  }
  const std::string toSend = ":" + _nick + "!~" + _user + "@" + _hostname +
                             " PRIVMSG " + target + " :" + text;
  _server->sendToChannel(targetChannel, toSend, this);
}

void Client::join(const std::vector<std::string> &msg) {
  if (msg.size() < 2) {
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  std::string target = msg[1];
  // TODO: multiple channels
  if (target[0] == ':') {
    target = target.substr(1);
  }
  if (target.empty()) {
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  if (target == "0") {
    std::vector<std::string> channels;
    channels.push_back("PART");
    for (ChannelList::iterator it = _channels.begin(); it != _channels.end();
         ++it) {
      channels.push_back(it->first);
    }
    part(channels);
    return;
  }
  // Validated channel name
  if (!Channel::isValidName(target)) {
    createMessage(Server::ERR_NOSUCHCHANNEL, target);
    // TODO: make sure this is the right error code
    return;
  }

  Channel *targetChannel = findChannel(_server->getChannels(), target);
  if (targetChannel == NULL) {
    targetChannel = new Channel(target, _server);
    _server->addChannel(targetChannel);
  }
  if (findChannel(_channels, target) != NULL) {
    return;  // Already in the channel
  }
  if (targetChannel->isInviteOnly()) {
    if (findClient(targetChannel->getInvited(), _clientFd) == NULL) {
      createMessage(Server::ERR_INVITEONLYCHAN, target);
      return;
    }
  }
  if (targetChannel->isLimited() &&
      targetChannel->getClients().size() >= targetChannel->getLimit()) {
    createMessage(Server::ERR_CHANNELISFULL, target);
    return;
  }
  if (targetChannel->isPassRequired()) {
    if (msg.size() < 3) {
      // TODO: check if this is the right error code
      createMessage(Server::ERR_NEEDMOREPARAMS, "");
      return;
    }
    const std::string &pass = msg[2];
    if (pass != targetChannel->getPassword()) {
      createMessage(Server::ERR_PASSWDMISMATCH, pass);
      return;
    }
  }
  // TODO: check the channel limit for user ERR_TOOMANYCHANNELS
  targetChannel->addClient(this);
  _channels[target] = targetChannel;
  // If a JOIN is successful, the user receives a JOIN message as
  // confirmation and is then sent the channel's topic (using RPL_TOPIC) and
  // the list of users who are on the channel (using RPL_NAMREPLY), which
  // MUST include the user joining.

  // TODO: send confirmation to client
  // RPL_TOPIC;
  // RPL_NAMREPLY;

  _server->sendToChannel(targetChannel, ":" + _nick + "!~" + _user + "@" +
                                            _hostname + " JOIN " + target);
}

void Client::part(const std::vector<std::string> &msg) {
  if (msg.size() < 2) {
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  std::string target = msg[1];
  if (target[0] == ':') {
    target = target.substr(1);
  }
  if (target.empty()) {
    createMessage(Server::ERR_NEEDMOREPARAMS, msg[0]);
    return;
  }
  // TODO: multiple channels
  Channel *channel = findChannel(_server->getChannels(), target);
  if (channel == NULL) {
    createMessage(Server::ERR_NOSUCHCHANNEL, target);
    return;
  }
  if (findChannel(_channels, target) == NULL) {
    createMessage(Server::ERR_NOTONCHANNEL, target);
    return;
  }
  // TODO: check default part message
  std::string reason = "Client left the channel";
  if (msg.size() > 2) {
    reason = msg[2];
    if (reason[0] == ':') {
      reason = reason.substr(1);
    }
  }
  _server->sendToChannel(channel, ":" + _nick + "!~" + _user + "@" + _hostname +
                                      " PART " + target + " :" + reason);
  channel->removeClient(_clientFd);
  // TODO: check if we need to confirm
  _channels.erase(target);
}

void Client::kick(const std::vector<std::string> &msg) { (void)msg; }
void Client::invite(const std::vector<std::string> &msg) { (void)msg; }
void Client::topic(const std::vector<std::string> &msg) { (void)msg; }
void Client::mode(const std::vector<std::string> &msg) { (void)msg; }
void Client::names(const std::vector<std::string> &msg) { (void)msg; }
void Client::list(const std::vector<std::string> &msg) { (void)msg; }
// send RPL_LIST for each channel and then RPL_LISTEND

void Client::appendToOutBuffer(const std::string &msg) {
  _outBuffer += msg + "\r\n";
}

void Client::createMessage(ERR error_code, const std::string &param) {
  std::stringstream ss;
  ss << ":" << _server->getName() << " " << error_code;
  ss << " " << (_isAuthenticated ? _nick : "*") << " ";
  ss << param << (param.empty() ? "" : " ") << ":";

  if (Server::ERRORS.find(error_code) != Server::ERRORS.end()) {
    ss << Server::ERRORS.at(error_code);
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
       << "available user modes, available channel modes";  // TODO
  } else if (response_code == Server::RPL_LUSERCLIENT) {
    ss << ":There are " << _server->getClients().size()
       << " users and 0 invisible on this server";
    // TODO: check what's invisible
  } else if (response_code == Server::RPL_LUSEROP) {
    ss << ":There are 0 operators online";
    // TODO: check what's operator on a server
  } else if (response_code == Server::RPL_LUSERUNKNOWN) {
    ss << ":There are 0 unknown connections";  // TODO: check what's unknown
  } else if (response_code == Server::RPL_LUSERCHANNELS) {
    ss << ":There are " << _server->getChannels().size() << " channels created";
  } else if (response_code == Server::RPL_LUSERME) {
    ss << ":I have a total of " << _server->getClients().size() << " clients";
  } else if (response_code == Server::RPL_LISTEND) {
    ss << ":End of LIST";
  } else if (response_code == Server::RPL_TIME) {
    ss << _server->getName() << " :" << get_time(_server->getCreatedAt());
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
       << "* :" << targetClient->getRealName();
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
  } else if (response_code == Server::RPL_ENDOFWHOIS) {
    ss << ":End of WHOIS list";
    _server->sendToClient(this, ss.str());
  } else {
    ss << ":Unknown response code";
  }
}

void Client::createMessage(RPL response_code, Channel *targetChannel) {
  if (targetChannel == NULL) {
    return;
  }
  std::stringstream ss;
  ss << ":" << _server->getName() << " " << response_code << " " << _nick << " "
     << targetChannel->getName() << " ";

  if (response_code == Server::RPL_LIST) {
    ss << ":" << targetChannel->getName() << " "
       << targetChannel->getClients().size() << " :"
       << targetChannel->getTopic();
  } else if (response_code == Server::RPL_CHANNELMODEIS) {
    // ss << targetChannel->getMode(); TODO
  } else if (response_code == Server::RPL_NOTOPIC) {
    ss << ":No topic is set";
  } else if (response_code == Server::RPL_TOPIC) {
    ss << ":" << targetChannel->getTopic();
  } else {
    ss << ":Unknown response code";
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
