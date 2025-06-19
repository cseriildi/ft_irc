#pragma once

#include <map>
#include <string>

class Server;
class Client;

class Channel {
 public:
  Channel(const std::string &name, Server *server);
  ~Channel();

  std::string join(int clientFd, const std::string &pass = "");
  std::string part(int clientFd);
  std::string kick(int clientFd, const std::string &nick,
                   const std::string &reason = "");
  std::string invite(int clientFd, const std::string &nick);
  std::string topic(int clientFd, const std::string &topic = "");
  std::string mode(int clientFd, const std::string &modes);
  std::string privmsg(int clientFd, const std::string &msg);

  std::map<int, Client *> getClients() const;
  std::map<int, Client *> getOperators() const;
  std::string getName() const;
  std::string getTopic() const;
  std::string getPassword() const;
  bool isInviteOnly() const;
  bool isTopicSet() const;
  bool isPassRequired() const;
  std::string getPass() const;
  bool isLimited() const;
  int getLimit() const;
  void setTopic(const std::string &topic);
  void setPassword(const std::string &password);
  void setInviteOnly(bool inviteOnly);
  void setTopicSet(bool topicSet);
  void setPassRequired(bool passRequired);
  void setPass(const std::string &pass);
  void setLimited(bool limited);
  void setLimit(int limit);
  void addClient(Client *client);
  void removeClient(int clientFd);
  void addOperator(Client *client);
  void removeOperator(int clientFd);

 private:
  Channel();
  Channel(const Channel &other);
  Channel &operator=(const Channel &other);

  std::string _name;
  std::string _topic;
  std::string _password;
  bool _isInviteOnly;
  bool _topicSet;
  bool _passRequired;
  std::string _pass;
  bool _isLimited;
  int _limit;
  std::map<int, Client *> _clients;
  std::map<int, Client *> _operators;
  Server *_server;
};
