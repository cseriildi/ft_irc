#include "Client.hpp"
#include "utils.hpp"
#include <cerrno>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <cstring>
#include <sys/socket.h>
#include <sys/types.h>
#include "Server.hpp"
#include <vector>

const std::map<std::string, CommandFunction> Client::COMMANDS = Client::init_commands_map();

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
	commands["PRIVMSG"] = &Client::privmsg;
	//TODO: add rest of commands
	return commands;
}

Client::Client(int sockfd, Server* server) :
											_clientFd(sockfd),
											_mode(0),
											_isPassSet(false),
											_isNickSet(false),
											_isUserSet(false),
											_isAuthenticated(false),
											_server(server) {}

Client::~Client() {}

const std::string& Client::getNick() const {return _nick;}
const std::string& Client::getUser() const {return _user;}
int Client::getMode() const {return _mode;}
const std::string& Client::getHostname() const {return _hostname;}
const std::string& Client::getRealName() const {return _realName;}
const std::string& Client::getPassword() const {return _password;}
bool Client::isPassSet() const {return _isPassSet;}
bool Client::isNickSet() const {return _isNickSet;}
bool Client::isUserSet() const {return _isUserSet;}
bool Client::isAuthenticated() const {return _isAuthenticated;}
int Client::getClientFd() const {return _clientFd;}

void Client::receive() {
	char buffer[BUFFER_SIZE];
	memset(buffer, 0, sizeof(buffer)); //NOLINT

	const ssize_t received = recv(_clientFd, buffer, sizeof(buffer) - 1, 0); //NOLINT
	if (received == 0) {
		throw std::runtime_error("Client disconnected");
	}
	if (received == -1) {
		throw std::runtime_error("Error receiving data: " + std::string(strerror(errno)));
	}
	_inBuffer.append(buffer, received); //NOLINT

	size_t pos = 0;
	while ((pos = _inBuffer.find("\r\n")) != std::string::npos) {
		std::string const line = _inBuffer.substr(0, pos);
		handle(line);
		_inBuffer.erase(0, pos + 2);
	}
}


void Client::answer() {
	std::cout << "Answering client: " << _outBuffer << '\n';
	while (!_outBuffer.empty()) {
		const ssize_t sent = send(_clientFd, _outBuffer.c_str(), _outBuffer.length(), 0); //NOLINT
		if (sent == -1) {
			throw std::runtime_error("Error sending data: " + std::string(strerror(errno)));
		}
		_outBuffer.erase(0, sent);
	}
}

bool Client::wantsToWrite() const {
	return !_outBuffer.empty();
}

void Client::handle(const std::string &msg) {

	if (msg.empty()) {
		return;
	}
	//TODO: think about leading spaces
	std::vector<std::string> parsed = split(msg);
	
	std::map<std::string, CommandFunction>::const_iterator fn = COMMANDS.find(parsed[0]);
	if (fn == COMMANDS.end()) {
		createMessage(Server::ERR_UNKNOWNCOMMAND);
		return;
	}
	CommandFunction command = fn->second;
	(this->*command)(parsed);
}


void Client::pass(const std::vector<std::string> &msg) {
	if (_isPassSet) {
		createMessage(Server::ERR_ALREADYREGISTRED);
		return;
	}
	if (msg.size() < 2) {
		createMessage(Server::ERR_NEEDMOREPARAMS);
		return;
	}
	std::string password = msg[1];

	if (password[0] == ':')
		password = password.substr(1); //Maybe I can remove it in the split
	//TODO: think about space in password
	_password = password;
	_isPassSet = true;
}

void Client::nick(const std::vector<std::string> &msg) {
	if (msg.size() < 2) {
		createMessage(Server::ERR_NEEDMOREPARAMS);
		return;
	}
	std::string nick = msg[1];

	if (nick[0] == ':')
		nick = nick.substr(1);
	if (nick.empty())
	{
		createMessage(Server::ERR_NONICKNAMEGIVEN);
		return;
	}
	_nick = nick;
	if (nick.find(' ') != std::string::npos) {
		createMessage(Server::ERR_ERRONEUSNICKNAME);
		return;
	}
	if (!_server->isNicknameAvailable(this)) {
		createMessage(Server::ERR_NICKNAMEINUSE);
		return;
	}
	std::string oldNick = _nick;
	_isNickSet = true;
	if (_isAuthenticated) {
		_server->sendToClient(this, ":" + oldNick + "!" + _user + "@" + _hostname + " NICK :"+ _nick);
	}
	else
	{
		if (_isUserSet) {
			if (!_server->authenticate(this)) {
				createMessage(Server::ERR_NOTREGISTERED);
				return;
			}
		_isAuthenticated = true;
		//TODO: check welcome message
		_server->sendToClient(this, ":localhost 001 " + _nick + " :Welcome to ft_irc!");
		}
	}
}

void Client::user(const std::vector<std::string> &msg) {
	if (_isUserSet) {
		createMessage(Server::ERR_ALREADYREGISTRED);
		return;
	}
	if (msg.size() < 5) { //NOLINT
		createMessage(Server::ERR_NEEDMOREPARAMS);
		return;
	}

	_user =  msg[1];
	_mode = 0; //TODO: extract mode
	_hostname =  msg[3];

	std::string realname = msg[4];
	if (realname[0] == ':') {
		realname = realname.substr(1);
	}
	_realName = realname;
	_isUserSet = true;
	if (_isNickSet) {
		if (!_server->authenticate(this)) {
			createMessage(Server::ERR_NOTREGISTERED);
			return;
		}
		_isAuthenticated = true;
		//TODO: check welcome message
		_server->sendToClient(this, ":ircserv 001 " + _nick + " :Welcome to ft_irc!");
	}
}

void Client::cap(const std::vector<std::string> &msg) {
	if (msg.size() >= 2 && msg[1] == "LS") {
		_server->sendToClient(this, "CAP * LS :");
	} 
}

void Client::ping(const std::vector<std::string> &msg) {
	if (msg.size() < 2 || msg[1] == ":") {
		createMessage(Server::ERR_NEEDMOREPARAMS);
		return;
	}
	std::string server = msg[1];
	if (server[0] == ':') {
		server = server.substr(1);
	}
	//TODO:check out what do I have to do here
	_server->sendToClient(this, "PONG " + server);
}

void Client::quit(const std::vector<std::string> &msg) {
	std::string reason = "Client quit";
	if (msg.size() > 1) {
		reason = msg[1];
		if (reason[0] == ':') {
			reason = reason.substr(1);
		}
	}
	//TODO: check on this
	_server->sendToClient(this, ":" + _nick + "!" + _user + "@" + _hostname + " QUIT :" + reason);
	//TODO: clean up
}

void Client::who(const std::vector<std::string> &msg) {(void)msg;}
void Client::privmsg(const std::vector<std::string> &msg) {(void)msg;}
void Client::join(const std::vector<std::string> &msg) {(void)msg;}
void Client::part(const std::vector<std::string> &msg) {(void)msg;}
void Client::kick(const std::vector<std::string> &msg) {(void)msg;}
void Client::invite(const std::vector<std::string> &msg) {(void)msg;}
void Client::topic(const std::vector<std::string> &msg) {(void)msg;}
void Client::mode(const std::vector<std::string> &msg) {(void)msg;}
void Client::names(const std::vector<std::string> &msg) {(void)msg;}
void Client::list(const std::vector<std::string> &msg) {(void)msg;}


void Client::appendToOutBuffer(const std::string &msg) {
	_outBuffer += msg + "\r\n";
}

int Client::getFd() const {
	return _clientFd;
}

void Client::createMessage(const std::string &msg, Client::TargetType targetType, const std::string &target)
{
	(void)targetType;
	(void)target;

	_server->sendToClient(this, msg);
}

void Client::createMessage(ERR error_code)
{
	std::stringstream ss;
	ss << ":localhost " << error_code << " " << _nick << " :";

	//TODO: unknown command

	if (Server::ERRORS.find(error_code) != Server::ERRORS.end()) {
		ss << Server::ERRORS.at(error_code);
	} else {
		ss << "Unknown error";
	}
	_server->sendToClient(this, ss.str());
}
