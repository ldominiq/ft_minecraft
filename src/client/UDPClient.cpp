#include "UDPClient.hpp"

UDPClient::UDPClient(const char* server_ip) {
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

    // Get current flags
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        perror("fcntl F_GETFL failed");
        exit(EXIT_FAILURE);
    }

    // Add non-blocking flag
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        perror("fcntl F_SETFL failed");
        exit(EXIT_FAILURE);
    }

    // Clear and set server info
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        perror("Invalid address / Address not supported");
        exit(EXIT_FAILURE);
    }

	sendConnect(); //CONNECTS THE CLIENT TO SERVER AUTOMATICALLY WHEN STARTED. Will have to change when we have a menu. Wont work if server isn't running already as there's no retry.
}

UDPClient::~UDPClient() {
    close(sockfd);
}

void UDPClient::sendPacket(const Packet &pkt) {
	auto bytes = encodePacket(pkt);
	sendto(sockfd, bytes.data(), bytes.size(), 0, (sockaddr*)&servaddr, sizeof(servaddr));
}

//TODO remove! This is for testing
void UDPClient::sendMessage(const char* message) {
    sendto(sockfd, message, strlen(message), MSG_CONFIRM,
           (const struct sockaddr*)&servaddr, sizeof(servaddr));
}

void UDPClient::sendConnect() {

	NetConnect connectPkt;
	connectPkt.username  = "Stesve";

	// std::vector<uint8_t> bytes = encodePacket(connectPkt);
	sendPacket(connectPkt);
}

void UDPClient::receivePacket() {
    std::vector<uint8_t> buffer(MAXLINE);
    socklen_t addrlen = sizeof(servaddr);

    while (true) {
        ssize_t n = recvfrom(sockfd, buffer.data(), buffer.size(), 0,
                             reinterpret_cast<struct sockaddr*>(&servaddr), &addrlen);
        if (n < 0) {
            if (errno == EWOULDBLOCK || errno == EAGAIN) break; // no more packets
            perror("recvfrom error");
            break;
        }
        // pass the actual number of bytes received
        dispatch(buffer.data(), static_cast<size_t>(n));
    }
}

void UDPClient::dispatch(const uint8_t* data, size_t n)
{
    // 1. Decode packet from buffer (returns unique_ptr<Packet>)
    auto pkt = decodePacket(data, n);
	if (onPacket) onPacket({ std::move(pkt) });
}

// void UDPClient::receiveAccept()
// {
// 	app = std::make_unique<App>();

// }
