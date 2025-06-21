#pragma once

#include <ctime>
#include <string>
#include <vector>

#include "Channel.hpp"
#include "Client.hpp"

bool startsWith(const std::string &str, const std::string &prefix);
std::vector<std::string> split(const std::string &line);
std::string uppercase(const std::string &str);

Client *findClient(const ClientList &clients, int fd);
Client *findClient(const ClientList &clients, const std::string &nick);
Channel *findChannel(const ChannelList &channels, const std::string &name);

std::string get_time(std::time_t t);
