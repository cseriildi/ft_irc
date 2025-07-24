#include "Bot.hpp"
#include <arpa/inet.h>
#include <cstddef>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
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
    if (connect(_sockfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
        close(_sockfd);
        _sockfd = -1;
        throw std::runtime_error("Connection to server failed");
    }
    std::cout << "Bot connected to server on port " << port << "\n";
    const std::string message = "PASS " + password + "\r\n NICK Bot\r\n USER Bot 0 * :Bot\r\n JOIN #trivia\r\n";
    send(_sockfd, message.c_str(), message.size(), 0);
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
    run();
}

void Bot::run() const {
    char buffer[BUFFER_SIZE];
    while (true) {
        const ssize_t bytesRead = recv(_sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            std::cerr << "Connection lost or error occurred.\n";
            break;
        }

        buffer[bytesRead] = '\0';
        const std::string msg(buffer);

        if (msg.find("PING") != std::string::npos) {
            const size_t pingPos = msg.find("PING");
            const std::string response = "PONG" + msg.substr(pingPos + 4) + "\r\n";
            send(_sockfd, response.c_str(), response.size(), 0);
        }

        size_t triviaIndex = 0;
        std::srand(time(NULL));
        if (msg.find("PRIVMSG #trivia :") != std::string::npos) {
            triviaIndex = std::rand() % _trivia.size();
            const std::string reply = "PRIVMSG #trivia :" + _trivia[triviaIndex] + "\r\n";
            send(_sockfd, reply.c_str(), reply.size(), 0);
        }
    }
}

Bot::Bot() : _sockfd(-1) {}

Bot::~Bot() {
    if (_sockfd != -1) {
        close(_sockfd);
        _sockfd = -1;
        std::cout << "Bot socket closed\n";
    }
}

Bot::Bot(const Bot &other) : _sockfd(other._sockfd) {}

Bot &Bot::operator=(const Bot &other) {
    if (this != &other) {
        _sockfd = other._sockfd;
    }
    return *this;
}
