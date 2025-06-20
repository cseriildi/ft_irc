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

bool startsWith(const std::string &str, const std::string &prefix) {
  return str.size() >= prefix.size() &&
         uppercase(str).compare(0, prefix.size(), prefix) == 0;
}

std::vector<std::string> split(const std::string &line) {
  std::vector<std::string> result;
  if (line.empty()) {
    return result;
  }
  std::string token;
  size_t pos = 0;
  size_t const found_colon = line.find(':');

  size_t const end =
      (found_colon == std::string::npos) ? line.length() : found_colon;

  while (pos < end) {
    size_t found_space = line.find(' ', pos);
    if (found_space == std::string::npos || found_space >= end) {
      found_space = end;
    }
    token = line.substr(pos, found_space - pos);
    if (!token.empty()) {
      result.push_back(token);
    }
    pos = found_space + 1;
  }

  if (found_colon != std::string::npos) {
    result.push_back(line.substr(found_colon));
  }
  if (!result.empty()) result[0] = uppercase(result[0]);

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
  for (ClientList::const_iterator it = clients.begin(); it != clients.end();
       ++it) {
    // TODO: case insensitive comparison
    if (it->second->getNick() == nick) {
      return it->second;
    }
  }
  return NULL;
}

Channel *findChannel(const ChannelList &channels, const std::string &name) {
  const ChannelList::const_iterator it = channels.find(name);
  // TODO: case insensitive comparison
  if (it != channels.end()) {
    return it->second;
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
