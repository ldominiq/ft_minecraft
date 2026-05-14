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
#include <stdexcept>

#include <chrono>
#include <deque>
#include <fcntl.h>
#include <zstd.h>
#include <glm/glm.hpp>
#include <memory>
#include <sstream>

#include "Protocol.hpp"
#include "Network.hpp"
#include "ChunkRenderer.hpp"

class UDPClient {
private:
    int sockfd;
    char buffer[MAXLINE];
    struct sockaddr_in servaddr;

    struct DelayedPacket {
        std::chrono::steady_clock::time_point dispatchAt;
        std::vector<uint8_t> data;
    };
    float m_simLatencyMs = 0.0f;
    std::deque<DelayedPacket> m_receiveDelayQueue;

    ReliabilitySender   sendRel;  // client -> server
    ReliabilityReceiver recvRel;  // server -> client

    void sendRawBytes(const std::vector<uint8_t>& bytes);

public:
    UDPClient(const char* server_ip); // Constructor to set server IP
    ~UDPClient(); // Destructor to close socket

	std::function<void(const PacketPtr&)> onPacket;
	void setCallback(std::function<void(const PacketPtr&)> cb) { onPacket = std::move(cb); }

	void setSimulatedLatency(float ms) {
        if (ms <= 0.0f && m_simLatencyMs > 0.0f)
            m_receiveDelayQueue.clear(); // discard stale delayed packets
        m_simLatencyMs = ms;
    }
	float getSimulatedLatency() const  { return m_simLatencyMs; }

	void sendPacket(Packet &pkt);
	void sendConnect(const std::string &username);

	void receivePacket();
	void dispatch(const uint8_t* data, size_t n);

	void reliabilityKeepalive();
};

#endif
