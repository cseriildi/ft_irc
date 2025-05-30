#include "Channel.hpp"
#include <string>

Channel::Channel(const std::string &name, Server *server) : 
	_name(name),
	_isInviteOnly(false),
	_topicSet(false),
	_passRequired(false),
	_isLimited(false),
	_limit(0),
	_server(server) {}
	
Channel::~Channel() {}
