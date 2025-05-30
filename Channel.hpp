#pragma once

#include <map>
#include <string>

class Server;
class Client;

class Channel {
	public:

		Channel(const std::string &name, Server *server);
		~Channel();

		std::map<int, Client*> getClients() const;

		std::string join(int clientFd, const std::string &pass="");
		std::string part(int clientFd);
		std::string kick(int clientFd, const std::string &nick, const std::string &reason="");
		std::string invite(int clientFd, const std::string &nick);
		std::string topic(int clientFd, const std::string &topic="");
		std::string mode(int clientFd, const std::string &modes);
		std::string privmsg(int clientFd, const std::string &msg);

	private:

		Channel();
		Channel(const Channel &other);
		Channel &operator=(const Channel &other);

		std::string				_name;
		std::string				_topic;
		std::string				_password;
		bool					_isInviteOnly;
		bool					_topicSet;
		bool					_passRequired;
		std::string				_pass;
		bool					_isLimited;
		int						_limit;
		std::map<int, Client*>	_clients;
		std::map<int, Client*>	_operators;
		Server*					_server;
};
