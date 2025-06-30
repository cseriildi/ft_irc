#include <cctype>
#include <cstddef>
#include <ctime>
#include <string>
#include <vector>

#include "Channel.hpp"
#include "Client.hpp"

std::string uppercase(const std::string &str) {
  std::string result(str);
  for (std::string::iterator it = result.begin(); it != result.end(); ++it) {
    if (*it == '{') {
      *it = '[';
    } else if (*it == '|') {
      *it = '\\';
    } else if (*it == '^') {
      *it = '~';
    }
    *it = static_cast<char>(std::toupper(*it));
  }
  return result;
}

std::string lowercase(const std::string &str) {
  std::string result(str);
  for (std::string::iterator it = result.begin(); it != result.end(); ++it) {
    if (*it == '[') {
      *it = '{';
    } else if (*it == '\\') {
      *it = '|';
    } else if (*it == '~') {
      *it = '^';
    }
    *it = static_cast<char>(std::tolower(*it));
  }
  return result;
}

bool startsWith(const std::string &str, const std::string &prefix) {
  return str.size() >= prefix.size() &&
         uppercase(str).compare(0, prefix.size(), prefix) == 0;
}

std::vector<std::string> split(const std::string &line, char delimiter) {
  std::vector<std::string> result;
  if (line.empty()) {
    return result;
  }
  std::string token;
  size_t pos = 0;
  size_t const end = line.length();
  while (pos < end) {
    size_t found_space = line.find(delimiter, pos);
    if (found_space == std::string::npos || found_space >= end) {
      found_space = end;
    }
    token = line.substr(pos, found_space - pos);
    if (!token.empty()) {
      result.push_back(token);
    }
    pos = found_space + 1;
  }
  return result;
}

std::vector<std::string> parse(const std::string &line) {
  size_t const found_colon = line.find(" :");
  std::vector<std::string> result = split(line.substr(0, found_colon), ' ');

  if (found_colon != std::string::npos) {
    result.push_back(line.substr(found_colon + 1));
  }
  if (!result.empty()) {
    result[0] = uppercase(result[0]);
  }
  return result;
}

Client *findClient(const ClientList &clients, int fd) {
  const ClientList::const_iterator it = clients.find(fd);
  if (it != clients.end()) {
    return it->second;
  }
  return NULL;
}

Client *findClient(const ClientList &clients, const std::string &nick) {
  const std::string l_nick = lowercase(nick);
  for (ClientList::const_iterator it = clients.begin(); it != clients.end();
       ++it) {
    if (lowercase(it->second->getNick()) == l_nick) {
      return it->second;
    }
  }
  return NULL;
}

Channel *findChannel(const ChannelList &channels, const std::string &name) {
  const std::string l_name = lowercase(name);
  for (ChannelList::const_iterator it = channels.begin(); it != channels.end();
       ++it) {
    if (lowercase(it->second->getName()) == l_name) {
      return it->second;
    }
  }
  return NULL;
}

std::string get_time(std::time_t t) {
  std::string result = std::ctime(&t);
  if (result.empty()) {
    return "unknown time";
  }
  if (*(result.end() - 1) == '\n') {
    result.erase(result.end() - 1);
  }
  return result;
}
