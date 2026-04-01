#include "UDPClient.hpp"

UDPClient::UDPClient(const char* server_ip) {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        exit(EXIT_FAILURE);
    }
#endif
    // Create socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        perror("socket creation failed");
        exit(EXIT_FAILURE);
    }

#ifdef _WIN32
    u_long mode = 1;
    if (ioctlsocket(sockfd, FIONBIO, &mode) != 0) {
        perror("ioctlsocket failed");
        exit(EXIT_FAILURE);
    }
#else
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
#endif

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
            std::cerr << "recvfrom error: " << err << "\n";
#else
            if (errno == EWOULDBLOCK || errno == EAGAIN) break; // no more packets
            if (errno == ECONNREFUSED) {
                // ICMP Port Unreachable received, ignore it for UDP
                continue;
            }
            perror("recvfrom error");
#endif
            break;
        }

        if (m_simLatencyMs > 0.0f) {
            auto dispatchAt = std::chrono::steady_clock::now()
                + std::chrono::microseconds(static_cast<long long>(m_simLatencyMs * 1000.0f));
            m_receiveDelayQueue.push_back({dispatchAt,
                std::vector<uint8_t>(buffer.data(), buffer.data() + n)});
        } else {
            dispatch(buffer.data(), static_cast<size_t>(n));
        }
    }

    // Flush any packets whose simulated delay has expired
    auto now = std::chrono::steady_clock::now();
    while (!m_receiveDelayQueue.empty() &&
           m_receiveDelayQueue.front().dispatchAt <= now)
    {
        auto& pkt = m_receiveDelayQueue.front();
        dispatch(pkt.data.data(), pkt.data.size());
        m_receiveDelayQueue.pop_front();
    }
}

void UDPClient::dispatch(const uint8_t* data, size_t n)
{
    // 1. Decode packet from buffer (returns unique_ptr<Packet>)
    auto pkt = decodePacket(data, n);
	if (onPacket) onPacket(std::move(pkt));
}

