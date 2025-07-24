#pragma once

#include <arpa/inet.h>  // for send, recv

#include <ctime>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Server.hpp"

#define BUFFER_SIZE 512  // standard message size for IRC
#define CHANNEL_PREFIXES "#&+!"

class Channel;

typedef void (Client::*CommandFunction)(const std::vector<std::string> &);
typedef std::map<std::string, Channel *> ChannelList;
typedef std::vector<std::pair<char, char> > ModeChanges;

class Client {
 public:
  enum TargetType { CLIENT, CHANNEL, SERVER };

  typedef Server::ERR ERR;
  typedef Server::RPL RPL;

  static const std::map<std::string, CommandFunction> COMMANDS;

  static std::map<std::string, CommandFunction> init_commands_map();

  Client(int sockfd, Server *server);
  ~Client();

  // * COMMANDS *
  void handle(const std::string &msg);
  void pass(const std::vector<std::string> &msg);
  void nick(const std::vector<std::string> &msg);
  void user(const std::vector<std::string> &msg);
  void whois(const std::vector<std::string> &msg);
  void privmsg(const std::vector<std::string> &msg);
  void ping(const std::vector<std::string> &msg);
  void cap(const std::vector<std::string> &msg);
  void quit(const std::vector<std::string> &msg);
  void list(const std::vector<std::string> &msg);
  void server_time(const std::vector<std::string> &msg);

  // * CHANNEL COMMANDS *
  void join(const std::vector<std::string> &msg);
  void part(const std::vector<std::string> &msg);
  void kick(const std::vector<std::string> &msg);
  void invite(const std::vector<std::string> &msg);
  void topic(const std::vector<std::string> &msg);
  void mode(const std::vector<std::string> &msg);
  void names(const std::vector<std::string> &msg);

  // * GETTERS AND SETTERS *
  int getClientFd() const;
  const std::string &getNick() const;
  const std::string &getUser() const;
  const std::string &getHostname() const;
  const std::string &getRealName() const;
  const std::string &getPassword() const;
  time_t getJoinedAt() const;
  bool isPassSet() const;
  bool isNickSet() const;
  bool isUserSet() const;
  bool isAuthenticated() const;
  bool wantsToQuit() const;
  bool wantsToWrite() const;
  const ChannelList &getChannels() const;

  // * HELPERS *
  static bool isValidName(const std::string &name);
  void removeChannel(const std::string &name);
  void appendToOutBuffer(const std::string &msg);
  void leaveAllChannels();
  void broadcastToAllChannels(const std::string &msg,
                              const std::string &command = "");
  void joinChannel(Channel *channel);
  bool modeCheck(const std::string &modes, Channel *channel,
                 std::vector<std::string> &params);
  Channel *findChannelForMode(const std::vector<std::string> &msg);
  ModeChanges changeMode(std::vector<std::string> &params, Channel *channel,
                         const std::string &modes);

  // * COMMUNICATION *
  void receive();
  void answer();
  void createMessage(ERR error_code, const std::string &param = "",
                     const std::string &end = "");
  void createMessage(RPL response_code);
  void createMessage(RPL response_code, Client *targetClient);
  void createMessage(RPL response_code, Channel *targetChannel);
  void createMessage(RPL response_code, Channel *targetChannel,
                     Client *targetClient);

 private:
  Client();
  Client(const Client &other);
  Client &operator=(const Client &other);

  void _authenticate();
  void _broadcastNickChange(const std::string &newNick);
  void _messageClient(const std::vector<std::string> &msg);
  void _messageChannel(const std::vector<std::string> &msg);

  int _clientFd;
  std::string _nick;
  std::string _user;
  std::string _hostname;
  std::string _realName;
  std::string _password;
  time_t _joinedAt;
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
