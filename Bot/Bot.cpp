#include "Bot.hpp"

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

Bot::Bot(const std::string &port, const std::string &password) : _sockfd(-1) {
  int portNbr = 0;
  std::istringstream(port) >> portNbr;
  if (portNbr <= 0 || portNbr > MAX_PORT) {
    throw std::invalid_argument("Invalid port number: " + port);
  }

  struct sockaddr_in serverAddr = {};
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_port = htons(portNbr);
  serverAddr.sin_addr.s_addr = inet_addr("127.0.0.1");

  _sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (_sockfd < 0) {
    throw std::runtime_error("Socket creation failed");
  }
  if (connect(_sockfd, (struct sockaddr *)&serverAddr,  // NOLINT
              sizeof(serverAddr)) < 0) {
    close(_sockfd);
    _sockfd = -1;
    throw std::runtime_error("Connection to server failed");
  }
  std::cout << "Bot connected to server on port " << port << "\n";
  sendMessage(
      "PASS " + password +
      "\r\nNICK Bot\r\nUSER Bot 0 * :Bot\r\nJOIN #trivia\r\nTOPIC #trivia "
      ":Send any message to receive a ¡FUN! fact!\r\n");
  std::ifstream triviaFile("trivia.txt");
  if (!triviaFile.is_open()) {
    std::cerr << "Failed to open trivia file\n";
    return;
  }
  std::string line;
  while (std::getline(triviaFile, line) != 0) {
    _trivia.push_back(line);
  }
  triviaFile.close();
  std::srand(time(NULL));
  run();
}

void Bot::sendMessage(const std::string &message) const {
  send(_sockfd, message.c_str(), message.size(), 0);
}

void Bot::run() const {
  char buffer[BUFFER_SIZE];
  while (true) {
    const ssize_t bytesRead =
        recv(_sockfd, buffer, sizeof(buffer) - 1, 0);  // NOLINT
    if (bytesRead <= 0) {
      std::cerr << "Connection lost or error occurred.\n";
      break;
    }

    buffer[bytesRead] = '\0';       // NOLINT
    const std::string msg(buffer);  // NOLINT

    size_t triviaIndex = 0;
    if (msg.find("PRIVMSG #trivia :") != std::string::npos) {
      if (_trivia.empty()) {
        std::string const reply = "PRIVMSG #trivia :404 Trivia not found.\r\n";
        send(_sockfd, reply.c_str(), reply.size(), 0);
        continue;
      }
      triviaIndex = std::rand() % _trivia.size();
      const std::string reply =
          "PRIVMSG #trivia :" + _trivia[triviaIndex] + "\r\n";
      send(_sockfd, reply.c_str(), reply.size(), 0);
    } else if (msg.find("KICK #trivia Bot") != std::string::npos) {
      sendMessage(
          "JOIN #trivia\r\nTOPIC #trivia :Send any message to receive a ¡FUN! "
          "fact!\r\n");
      sendMessage("PRIVMSG #trivia :I'm back b*tches!\r\n");
    }
  }
}

Bot::~Bot() {
  if (_sockfd != -1) {
    close(_sockfd);
    _sockfd = -1;
    std::cout << "Bot socket closed\n";
  }
}
