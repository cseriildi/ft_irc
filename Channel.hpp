#pragma once

#include "Client.hpp"
#include "Message.hpp"
#include <map>
#include <string>

class Server;

class Channel {
	public:

		Channel(const std::string &name, Server *server);
		~Channel();
		
		Message join(int clientFd, const std::string &pass="");
		Message part(int clientFd);
		Message kick(int clientFd, const std::string &nick, const std::string &reason="");
		Message invite(int clientFd, const std::string &nick);
		Message topic(int clientFd, const std::string &topic="");
		Message mode(int clientFd, const std::string &modes);
		Message privmsg(int clientFd, const std::string &msg);
	
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
