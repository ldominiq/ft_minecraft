#ifndef UDPCLIENT_HPP
#define UDPCLIENT_HPP

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#define poll WSAPoll
#define close closesocket
typedef int ssize_t;
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <cstring>
#include <iostream>
#include <vector>
#include <functional>

#include <fcntl.h>
#include <zstd.h>
#include <glm/glm.hpp>
#include <memory>
#include <sstream>

#include "Protocol.hpp"
#include "ChunkRenderer.hpp"

class UDPClient {
private:
    int sockfd;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr;

public:
    UDPClient(const char* server_ip); // Constructor to set server IP
    ~UDPClient(); // Destructor to close socket

	std::function<void(const PacketPtr&)> onPacket;
	void setCallback(std::function<void(const PacketPtr&)> cb) { onPacket = std::move(cb); }

	void sendPacket(const Packet &pkt);
	void sendConnect();

	void receivePacket();
	void dispatch(const uint8_t* data, size_t n);
};

#endif
