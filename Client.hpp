#pragma once

#include <arpa/inet.h>  // for send, recv

#include <map>
#include <string>
#include <vector>

#include "Server.hpp"

#define BUFFER_SIZE 512  // standard message size for IRC

class Channel;

typedef void (Client::*CommandFunction)(const std::vector<std::string> &);
typedef std::map<std::string, Channel *> ChannelList;

class Client {
 public:
  enum TargetType { CLIENT, CHANNEL, SERVER };

  typedef Server::ERR ERR;
  typedef Server::RPL RPL;

  static const std::map<std::string, CommandFunction> COMMANDS;

  static std::map<std::string, CommandFunction> init_commands_map();

  Client(int sockfd, Server *server);
  ~Client();

  void handle(const std::string &msg);
  void receive();
  void answer();
  bool wantsToWrite() const;
  void appendToOutBuffer(const std::string &msg);
  int getFd() const;

  void createMessage(const std::string &msg, TargetType _targetType,
                     const std::string &target);
  void createMessage(ERR error_code, const std::string &command = "");
  void createMessage(const std::string &msg, const std::string &command = "");

  void createMessage(RPL response_code);
  void createMessage(RPL response_code, Client *targetClient);

  void pass(const std::vector<std::string> &msg);
  void nick(const std::vector<std::string> &msg);
  void user(const std::vector<std::string> &msg);
  void who(const std::vector<std::string> &msg);
  void whois(const std::vector<std::string> &msg);
  void privmsg(const std::vector<std::string> &msg);
  void ping(const std::vector<std::string> &msg);

  // Channel comman
  void join(const std::vector<std::string> &msg);
  void part(const std::vector<std::string> &msg);
  void kick(const std::vector<std::string> &msg);
  void invite(const std::vector<std::string> &msg);
  void topic(const std::vector<std::string> &msg);
  void mode(const std::vector<std::string> &msg);
  void names(const std::vector<std::string> &msg);

  // Server commands
  void cap(const std::vector<std::string> &msg);
  void quit(const std::vector<std::string> &msg);
  void list(const std::vector<std::string> &msg);

  // getters
  int getClientFd() const;
  const std::string &getNick() const;
  const std::string &getUser() const;
  int getMode() const;
  const std::string &getHostname() const;
  const std::string &getRealName() const;
  const std::string &getPassword() const;
  bool isPassSet() const;
  bool isNickSet() const;
  bool isUserSet() const;
  bool isAuthenticated() const;
  bool wantsToQuit() const;
  const ChannelList &getChannels() const;

  static bool isValidName(const std::string &name);

 private:
  // Instance of IRC interpeter, called with a string, returns a string
  Client();
  Client(const Client &other);
  Client &operator=(const Client &other);

  void _authenticate();
  void _broadcastNickChange(const std::string &newNick);

  int _clientFd;
  std::string _nick;
  std::string _user;
  int _mode;  // TODO: check what does this mean: bit 2 - w, bit 3 - i
              // (https://www.rfc-editor.org/rfc/rfc2812.html#section-3.1.5)
  std::string _hostname;
  std::string _realName;
  std::string _password;

  bool _isPassSet;
  bool _isNickSet;
  bool _isUserSet;
  bool _isAuthenticated;  // true after pass, nick, user
  bool _wantsToQuit;
  Server *_server;
  std::string _inBuffer;
  std::string _outBuffer;
  ChannelList _channels;
};
