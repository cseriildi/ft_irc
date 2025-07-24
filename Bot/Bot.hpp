#pragma once

#include <string>

#define BUFFER_SIZE 512  // standard message size for IRC
#define MAX_PORT 65535

class Bot {
    public:
        Bot(const std::string &port, const std::string &password);
        ~Bot();
    private:
        Bot();
        Bot(const Bot &other);
        Bot &operator=(const Bot &other);
        void run() const;

        int _sockfd;
};
