#include "UDPClient.hpp"

UDPClient::UDPClient(const char* server_ip) : sockfd(-1) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        throw std::runtime_error("WSAStartup failed");
    }
#endif
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
#ifdef _WIN32
        WSACleanup();
#endif
        throw std::runtime_error("socket creation failed");
    }

#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(sockfd, FIONBIO, &mode) != 0) {
        close(sockfd);
        WSACleanup();
        sockfd = -1;
        throw std::runtime_error("ioctlsocket failed");
    }
#else
    // Get current flags
    int flags = fcntl(sockfd, F_GETFL, 0);
    if (flags == -1) {
        close(sockfd);
        sockfd = -1;
        throw std::runtime_error("fcntl F_GETFL failed");
    }

    // Add non-blocking flag
    if (fcntl(sockfd, F_SETFL, flags | O_NONBLOCK) == -1) {
        close(sockfd);
        sockfd = -1;
        throw std::runtime_error("fcntl F_SETFL failed");
    }
#endif

    // Clear and set server info
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, server_ip, &servaddr.sin_addr) <= 0) {
        close(sockfd);
#ifdef _WIN32
        WSACleanup();
#endif
        sockfd = -1;
        throw std::runtime_error(std::string("Invalid address: ") + server_ip);
    }

    std::cout << "Connecting to server at " << server_ip << ":" << PORT << "..." << std::endl;

	sendConnect(); //CONNECTS THE CLIENT TO SERVER AUTOMATICALLY WHEN STARTED. Will have to change when we have a menu. Wont work if server isn't running already as there's no retry.
}

UDPClient::~UDPClient() {
    close(sockfd);
#ifdef _WIN32
    WSACleanup();
#endif
}

void UDPClient::sendPacket(const Packet &pkt) {
	auto bytes = encodePacket(pkt);
	sendto(sockfd, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0, (sockaddr*)&servaddr, sizeof(servaddr));
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
        ssize_t n = recvfrom(sockfd, reinterpret_cast<char*>(buffer.data()), static_cast<int>(buffer.size()), 0,
                             reinterpret_cast<struct sockaddr*>(&servaddr), &addrlen);
        if (n < 0) {
#ifdef _WIN32
            int err = WSAGetLastError();
            if (err == WSAEWOULDBLOCK) break;
            if (err == WSAECONNRESET) {
                // Connection reset by peer, prevent server crash
                // TODO: handle correctly
                continue;
            }
            std::cerr << "[Network] recvfrom error: " << err << "\n";
#else
            if (errno == EWOULDBLOCK || errno == EAGAIN) break; // no more packets
            if (errno == ECONNREFUSED) {
                // ICMP Port Unreachable received, ignore it for UDP
                continue;
            }
            perror("[Network] recvfrom error");
#endif
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
    if (pkt) {
        if (pkt->type == PacketType::NET_ACCEPT) {
            std::cout << "[Network] Successfully decoded NET_ACCEPT packet\n";
        }
		if (onPacket) onPacket({ std::move(pkt) });
	} else {
        std::cerr << "[Network] Failed to decode packet of " << n << " bytes. First byte: " << (n > 0 ? (int)data[0] : -1) << "\n";
    }
}

