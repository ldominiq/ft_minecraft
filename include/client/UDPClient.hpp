#ifndef UDPCLIENT_HPP
#define UDPCLIENT_HPP

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <vector>

#include "Protocol.hpp"
#include "Chunk.hpp"

#include <fcntl.h>
#include <zstd.h>
#include <glm/glm.hpp>
#include <memory>
#include <sstream>

#define PORT 1234

class UDPClient {
private:
    int sockfd;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr;

	std::function<void(const PacketPtr&)> onPacket;

public:
    UDPClient(const char* server_ip); // Constructor to set server IP
    ~UDPClient(); // Destructor to close socket

	void setCallback(std::function<void(const PacketPtr&)> cb) { onPacket = std::move(cb); }

	void sendPacket(const Packet &pkt);
    void sendMessage(const char* message);
	void sendConnect();

	void receivePacket();
	void dispatch(const uint8_t* data, size_t n);
	void receiveAccept();
};

#endif
