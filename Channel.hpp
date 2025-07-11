#pragma once

#include <cstddef>
#include <map>
#include <string>

class Server;
class Client;

typedef std::map<int, Client *> ClientList;

class Channel {
 public:
  Channel(const std::string &name, Server *server);
  ~Channel();

  void kick(int clientFd, const std::string &nick,
            const std::string &reason = "");
  void invite(int clientFd, const std::string &nick);
  void topic(int clientFd, const std::string &topic = "");
  void mode(int clientFd, const std::string &modes);
  void privmsg(int clientFd, const std::string &msg);

  ClientList getClients() const;
  ClientList getOperators() const;
  ClientList getInvited() const;
  std::string getName() const;
  std::string getTopic() const;
  std::string getPassword() const;
  std::string getMode(Client *client) const;
  bool isInviteOnly() const;
  bool isTopicOperOnly() const;
  bool isTopicSet() const;
  bool isPassRequired() const;
  std::string getPass() const;
  bool isLimited() const;
  size_t getLimit() const;
  void setTopic(const std::string &topic);
  void setPassword(const std::string &password);
  void setInviteOnly(bool inviteOnly);
  void setTopicSet(bool topicSet);
  void setPassRequired(bool passRequired);
  void setPass(const std::string &pass);
  void setLimited(bool limited);
  void setLimit(size_t limit);
  void setTopicOperOnly(bool topicOperOnly);

  void addClient(Client *client);
  void removeClient(int clientFd);
  void addOperator(Client *client);
  void removeOperator(int clientFd);
  void addInvited(Client *client);

  static bool isValidName(const std::string &name);

 private:
  Channel();
  Channel(const Channel &other);
  Channel &operator=(const Channel &other);

  std::string _name;
  std::string _topic;
  std::string _password;
  bool _isInviteOnly;
  bool _topicOperOnly;
  bool _topicSet;
  bool _passRequired;
  std::string _pass;
  bool _isLimited;
  size_t _limit;
  ClientList _clients;
  ClientList _operators;
  ClientList _invited;
  Server *_server;
};
