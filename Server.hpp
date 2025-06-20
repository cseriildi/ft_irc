#pragma once

#include <cstddef>
#include <map>
#include <string>
#include <vector>

#include "Channel.hpp"

#define BACKLOG 10
#define MAX_CLIENTS 100
#define TIMEOUT 5000  // poll will block for this long unless an event occurs

typedef std::map<int, Client *> ClientList;
typedef std::map<std::string, Channel *> ChannelList;

class Client;
class Server {
 public:
  enum RPL {
    RPL_WELCOME = 001,
    RPL_YOURHOST = 002,
    RPL_CREATED = 003,
    RPL_MYINFO = 004,
    RPL_UMODEIS = 221,
    RPL_LUSERCLIENT = 251,
    RPL_LUSEROP = 252,
    RPL_LUSERUNKNOWN = 253,
    RPL_LUSERCHANNELS = 254,
    RPL_LUSERME = 255,
    RPL_WHOISUSER = 311,
    RPL_WHOISSERVER = 312,
    RPL_WHOISOPERATOR = 313,
    RPL_WHOISIDLE = 317,
    RPL_ENDOFWHOIS = 318,
    RPL_WHOISCHANNELS = 319,
    RPL_LIST = 322,
    RPL_LISTEND = 323,
    RPL_CHANNELMODEIS = 324,
    RPL_NOTOPIC = 331,
    RPL_TOPIC = 332,
    RPL_INVITING = 341,
    RPL_WHOREPLY = 352,
    RPL_ENDOFWHO = 315,
    RPL_NAMREPLY = 353,
    RPL_ENDOFNAMES = 366,
    RPL_YOUREOPER = 381
  };

  enum ERR {
    ERR_NOSUCHNICK = 401,
    ERR_NOSUCHCHANNEL = 403,
    ERR_CANNOTSENDTOCHAN = 404,
    ERR_TOOMANYTARGETS = 407,
    ERR_NORECIPIENT = 411,
    ERR_NOTEXTTOSEND = 412,
    ERR_NOTOPLEVEL = 413,
    ERR_WILDTOPLEVEL = 414,
    ERR_BADMASK = 415,
    ERR_UNKNOWNCOMMAND = 421,
    ERR_NONICKNAMEGIVEN = 431,
    ERR_ERRONEUSNICKNAME = 432,
    ERR_NICKNAMEINUSE = 433,
    ERR_USERNOTINCHANNEL = 441,
    ERR_NOTONCHANNEL = 442,
    ERR_USERONCHANNEL = 443,
    ERR_NOTREGISTERED = 451,
    ERR_NEEDMOREPARAMS = 461,
    ERR_ALREADYREGISTRED = 462,
    ERR_PASSWDMISMATCH = 464,
    ERR_KEYSET = 467,
    ERR_CHANNELISFULL = 471,
    ERR_UNKNOWNMODE = 472,
    ERR_INVITEONLYCHAN = 473,
    ERR_BADCHANNELKEY =
        475,  // TODO: check what if a client gets kicked from a channel
    ERR_CHANOPRIVSNEEDED = 482,
    ERR_UMODEUNKNOWNFLAG = 501,
    ERR_USERSDONTMATCH = 502

  };

  static const std::map<ERR, std::string> ERRORS;

  Server(const std::string &port = "6667", const std::string &password = "");
  ~Server();

  void run();
  void sendToClient(Client *client, const std::string &msg);
  void sendToChannel(Channel *channel, const std::string &msg,
                     Client *sender = NULL);

  static std::map<Server::ERR, std::string> init_error_map();
  bool isNicknameAvailable(Client *user) const;

  // getters
  const std::string &getName() const;
  const std::string &getPort() const;
  const std::string &getPassword() const;
  bool isPassRequired() const;
  const ChannelList &getChannels() const;
  const ClientList &getClients() const;

  void removeChannel(const std::string &name);
  void removeClient(int cfd);
  void addChannel(Channel *channel);
  void addClient(Client *client);

 private:
  Server();
  Server(const Server &other);
  Server &operator=(const Server &other);

  void _cleanup();
  static int _bindAndListen(const struct addrinfo *res);
  void _addPollFd(int fd, short events);
  void _handleNewConnection(int sockfd);
  bool _handleClientActivity(size_t index);
  void _handlePollEvents();

  std::string _port;
  int _sockfdIpv4;
  int _sockfdIpv6;
  struct addrinfo *_res;
  std::vector<struct pollfd> _pollFds;  // vector of the fds we are polling
  ClientList _clients;                  // with client_fd as key
  ChannelList _channels;                // with channel name as key
  std::string _name;
  bool _isPassRequired;
  std::string _password;
};
