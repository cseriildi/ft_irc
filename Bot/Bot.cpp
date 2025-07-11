#include "Bot.hpp"
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>
#include <sstream>

Bot::Bot(const std::string &port, const std::string &password) : _sockfd(-1) {

    std::cout << "Bot constructor called with port: " << port << " and password: " << password << "\n";
    int portNbr = 0;
    std::istringstream(port) >> portNbr;
    if (portNbr <= 0 || portNbr > 65535) {
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
    std::string message = "PASS " + password + "\r\n NICK Bot\r\n USER Bot 0 * :Bot\r\n JOIN #weather\r\n";
    send(_sockfd, message.c_str(), message.size(), 0);
    std::cout << "Bot sent initial message to server\n";
    run();
}

void Bot::run() {
    char buffer[BUFFER_SIZE];
    while (true) {
        ssize_t bytesRead = recv(_sockfd, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            std::cerr << "Connection lost or error occurred.\n";
            break;
        }

        buffer[bytesRead] = '\0';
        std::string msg(buffer);

        std::cout << "Received:\n" << msg << std::endl;

        // Respond to PING to stay connected
        if (msg.find("PING") != std::string::npos) {
            size_t pingPos = msg.find("PING");
            std::string response = "PONG" + msg.substr(pingPos + 4) + "\r\n";
            send(_sockfd, response.c_str(), response.size(), 0);
            std::cout << "Sent: " << response;
        }

        // If someone sends a message to #weather
        if (msg.find("PRIVMSG #weather :") != std::string::npos) {
            std::string reply = "PRIVMSG #weather :weather\r\n";
            send(_sockfd, reply.c_str(), reply.size(), 0);
            std::cout << "Sent: " << reply;
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

Bot::Bot(const Bot &other) { (void)other; }

Bot &Bot::operator=(const Bot &other) {
    if (this != &other) {
    }
    return *this;
}
