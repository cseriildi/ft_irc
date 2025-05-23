# About the cub3d project

We collaborated on this with [Daniel](https://github.com/Csicsi).

## Summary
This project consists of building a simplified IRC *(Internet Relay Chat)* server in C++ using low-level socket programming. The server handles multiple simultaneous client connections and supports a subset of the IRC protocol, including some basic commands. It manages user sessions, channel memberships, and real-time message broadcasting. Networking is implemented using POSIX socket functions and poll for handling multiple clients efficiently.

- **Allowed external functions**: `socket`, `close`, `setsockopt`, `getsockname`, `getprotobyname`, `gethostbyname`, `getaddrinfo`, `freeaddrinfo`, `bind`, `connect`, `listen`, `accept`, `htons`, `htonl`, `ntohs`, `ntohl`, `inet_addr`, `inet_ntoa`, `send`, `recv`, `signal`, `sigaction`, `lseek`, `fstat`, `fcntl`, `poll` *(or equivalent)*

## Mandatory Part

## Bonus Part
