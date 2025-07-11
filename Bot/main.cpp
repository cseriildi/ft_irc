#include "Bot.hpp"
#include <iostream>

int main(int argc, char **argv) {
    if (argc != 3) {
        std::cerr << "Usage: ./bot <port> <password>\n";
        return 1;
    }

    try {
        Bot bot(argv[1], argv[2]);
    } catch (const std::exception &e) {
        std::cerr << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Unknown exception\n";
        return 1;
    }

    return 0;
}