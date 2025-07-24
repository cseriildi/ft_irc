#include "Client.hpp"

#include <sys/socket.h>
#include <sys/types.h>

#include <cctype>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <map>
#include <string>

#include "Channel.hpp"
#include "Server.hpp"

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
