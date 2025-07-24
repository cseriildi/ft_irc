#include <sys/socket.h>
#include <sys/types.h>

#include <cerrno>
#include <cstring>
#include <ctime>
#include <iomanip>
#include <iostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "Channel.hpp"
#include "Client.hpp"
#include "utils.hpp"

void Client::receive() {
  char buffer[BUFFER_SIZE];
  memset(buffer, 0, sizeof(buffer));  // NOLINT

  const ssize_t received =
      recv(_clientFd, buffer, sizeof(buffer) - 1, 0);  // NOLINT
  if (received == 0) {
    throw std::runtime_error("Client disconnected");
  }
  if (received == -1) {
    throw std::runtime_error("Error receiving data: " +
                             std::string(strerror(errno)));
  }
  _inBuffer.append(buffer, received);  // NOLINT

  size_t pos = 0;
  while ((pos = _inBuffer.find("\r\n")) != std::string::npos) {
    std::string const line = _inBuffer.substr(0, pos);
    handle(line);
    _inBuffer.erase(0, pos + 2);
  }
}

void Client::answer() {
#ifdef DEBUG
  std::cout << "> Answered: " << _outBuffer << '\n';
#endif

  while (!_outBuffer.empty()) {
    const ssize_t sent =
        send(_clientFd, _outBuffer.c_str(), _outBuffer.length(), 0);  // NOLINT
    if (sent == -1) {
      throw std::runtime_error("Error sending data: " +
                               std::string(strerror(errno)));
    }
    _outBuffer.erase(0, sent);
  }
}

// * MESSAGES *

void Client::createMessage(ERR error_code, const std::string &param,
                           const std::string &end) {
  std::stringstream ss;
  ss << ":" << _server->getName() << " " << error_code;
  ss << " " << (_isAuthenticated ? _nick : "*") << " ";
  ss << param << (param.empty() ? "" : " ") << ":";

  if (Server::ERRORS.find(error_code) != Server::ERRORS.end()) {
    ss << Server::ERRORS.at(error_code);
    if (!end.empty()) {
      ss << " " << end;
    }
  } else {
    ss << "Unknown error";
  }
  _server->sendToClient(this, ss.str());
}

void Client::createMessage(RPL response_code) {
  std::stringstream ss;
  ss << ":" << _server->getName() << " " << std::setw(3) << std::setfill('0')
     << response_code << " " << _nick << " ";
  if (response_code == Server::RPL_WELCOME) {
    ss << ":Welcome to the Internet Relay Network " << _nick << "!~" << _user
       << "@" << _hostname;
  } else if (response_code == Server::RPL_YOURHOST) {
    ss << ":Your host is " << _server->getName() << ", running version 1.0";
  } else if (response_code == Server::RPL_CREATED) {
    ss << ":This server was created " << get_time(_server->getCreatedAt());
  } else if (response_code == Server::RPL_MYINFO) {
    ss << _server->getName() << " 1.0 "
       << "- " << "itklo";
  } else if (response_code == Server::RPL_LISTEND) {
    ss << ":End of LIST";
  } else if (response_code == Server::RPL_TIME) {
    ss << _server->getName() << " :" << get_time(std::time(NULL));
  } else if (response_code == Server::RPL_ENDOFWHOIS) {
    ss << ":End of WHOIS list";
  } else if (response_code == Server::RPL_ENDOFNAMES) {
    ss << ":End of NAMES list";
  } else {
    ss << ":Unknown response code";
  }
  _server->sendToClient(this, ss.str());
}

void Client::createMessage(RPL response_code, Client *targetClient) {
  std::stringstream ss;
  ss << ":" << _server->getName() << " " << response_code << " " << _nick << " "
     << targetClient->getNick() << " ";
  if (response_code == Server::RPL_WHOISUSER) {
    ss << "~" << targetClient->getUser() << " " << targetClient->getHostname()
       << " * :" << targetClient->getRealName();
  } else if (response_code == Server::RPL_WHOISCHANNELS) {
    ss << ":";
    const ChannelList &channels = targetClient->getChannels();
    for (ChannelList::const_iterator it = channels.begin();
         it != channels.end(); ++it) {
      if (findClient(it->second->getClients(), targetClient->_clientFd) !=
          NULL) {
        ss << "@";  // Channel operator
      }
      ss << it->first;
      ChannelList::const_iterator nextIt = it;
      ++nextIt;
      if (nextIt != channels.end()) {
        ss << " ";
      }
    }
  } else if (response_code == Server::RPL_WHOISSERVER) {
    ss << _server->getName() << " :ft_irc server";
  } else if (response_code == Server::RPL_WHOISIDLE) {
    ss << (time(NULL) - targetClient->getJoinedAt()) << " :seconds idle";
  } else {
    ss << ":Unknown response code";
  }
  _server->sendToClient(this, ss.str());
}

void Client::createMessage(RPL response_code, Channel *targetChannel) {
  if (targetChannel == NULL) {
    return;
  }
  std::stringstream ss;
  ss << ":" << _server->getName() << " " << response_code << " " << _nick
     << " ";

  if (response_code == Server::RPL_LIST) {
    ss << targetChannel->getName() << " " << targetChannel->getClients().size()
       << " :" << targetChannel->getTopic();
  } else if (response_code == Server::RPL_CHANNELMODEIS) {
    ss << targetChannel->getName() << " " << targetChannel->getMode(this);
  } else if (response_code == Server::RPL_NOTOPIC) {
    ss << targetChannel->getName() << " :No topic is set";
  } else if (response_code == Server::RPL_TOPIC) {
    ss << targetChannel->getName() << " :" << targetChannel->getTopic();
  } else if (response_code == Server::RPL_NAMREPLY) {
    const ClientList &clients = targetChannel->getClients();
    const ClientList &operators = targetChannel->getOperators();
    ss << "= " << targetChannel->getName() << " :";
    for (ClientList::const_iterator it = clients.begin(); it != clients.end();
         ++it) {
      if (it != clients.begin()) {
        ss << " ";
      }
      if (findClient(operators, it->first) != NULL) {
        ss << "@";  // Channel operator
      }
      ss << it->second->getNick();
    }
  } else if (response_code == Server::RPL_LISTEND) {
    ss << targetChannel->getName() << " :End of LIST";
  } else if (response_code == Server::RPL_ENDOFNAMES) {
    ss << targetChannel->getName() << " :End of NAMES list";
  } else {
    ss << targetChannel->getName() << " :Unknown response code";
  }
  _server->sendToClient(this, ss.str());
}

void Client::createMessage(RPL response_code, Channel *targetChannel,
                           Client *targetClient) {
  if (targetChannel == NULL || targetClient == NULL) {
    return;
  }
  std::stringstream ss;
  ss << ":" << _server->getName() << " " << response_code << " " << _nick << " "
     << targetChannel->getName() << " ";
  if (response_code == Server::RPL_INVITING) {
    ss << targetClient->getNick();
  } else {
    ss << ":Unknown response code";
  }
  _server->sendToClient(this, ss.str());
}

void Client::broadcastToAllChannels(const std::string &msg,
                                    const std::string &command) {
  std::string reply =
      ":" + _nick + "!~" + _user + "@" + _hostname + " " + command;
  if (command == "PART") {
    for (ChannelList::const_iterator it = _channels.begin();
         it != _channels.end(); ++it) {
      _server->sendToChannel(it->second, reply + " " + it->first + " :" + msg);
    }
    return;
  }
  reply += " :" + msg;
  if (_channels.empty()) {
    _server->sendToClient(this, reply);
    return;
  }
  std::vector<int> fds;
  for (ChannelList::const_iterator it = _channels.begin();
       it != _channels.end(); ++it) {
    ClientList cl = it->second->getClients();
    for (ClientList::const_iterator cit = cl.begin(); cit != cl.end(); ++cit) {
      fds.push_back(cit->first);
    }
  }
  const ClientList clients = _server->getClients();
  const std::set<int> uniqueClients(fds.begin(), fds.end());
  for (std::set<int>::const_iterator it = uniqueClients.begin();
       it != uniqueClients.end(); ++it) {
    _server->sendToClient(findClient(clients, *it), reply);
  }
}

void Client::_messageClient(const std::vector<std::string> &msg) {
  const std::string &target = msg[1];
  const std::string &text = msg[2];
  Client *targetClient = findClient(_server->getClients(), target);
  if (targetClient == NULL) {
    createMessage(Server::ERR_NOSUCHNICK, target);
    return;
  }
  const std::string toSend = ":" + _nick + "!~" + _user + "@" + _hostname +
                             " PRIVMSG " + target + " :" + text;
  _server->sendToClient(targetClient, toSend);
}

void Client::_messageChannel(const std::vector<std::string> &msg) {
  const std::string &target = msg[1];
  const std::string &text = msg[2];
  Channel *targetChannel = findChannel(_server->getChannels(), target);
  if (targetChannel == NULL) {
    createMessage(Server::ERR_NOSUCHCHANNEL, target);
    return;
  }
  if (findChannel(_channels, target) == NULL) {
    createMessage(Server::ERR_CANNOTSENDTOCHAN, target);
    return;
  }
  const std::string toSend = ":" + _nick + "!~" + _user + "@" + _hostname +
                             " PRIVMSG " + target + " :" + text;
  _server->sendToChannel(targetChannel, toSend, this);
}
