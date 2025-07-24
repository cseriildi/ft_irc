#pragma once

#include <string>
#include <vector>

#define BUFFER_SIZE 512  // standard message size for IRC
#define MAX_PORT 65535

class Bot { // NOLINT
    public:
        Bot(const std::string &port, const std::string &password);
        ~Bot();
    private:
        void run() const;
        void sendMessage(const std::string &message) const;

        int _sockfd;
        std::vector<std::string> _trivia;
};
