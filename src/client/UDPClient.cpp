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

	sendConnect();
}

UDPClient::~UDPClient() {
    close(sockfd);
#ifdef _WIN32
    WSACleanup();
#endif
}

void UDPClient::sendRawBytes(const std::vector<uint8_t>& bytes) {
	sendto(sockfd, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0, (sockaddr*)&servaddr, sizeof(servaddr));
}

void UDPClient::sendPacket(Packet &pkt) {
	std::vector<uint8_t> bytes;
	if (hasFlag(pkt.flags, PacketFlags::Reliable)) {
		bytes = reliabilityStamp(sendRel, pkt);
	} else {
		bytes = encodePacket(pkt);
	}
	sendRawBytes(bytes);
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

        if (m_simLatencyMs > 0.0f) {
            auto dispatchAt = std::chrono::steady_clock::now()
                + std::chrono::microseconds(static_cast<long long>(m_simLatencyMs * 1000.0f));
            constexpr size_t kMaxDelayQueueSize = 512;
            if (m_receiveDelayQueue.size() >= kMaxDelayQueueSize)
                m_receiveDelayQueue.pop_front(); // drop oldest to bound memory
            m_receiveDelayQueue.push_back({dispatchAt,
                std::vector<uint8_t>(buffer.data(), buffer.data() + n)});
        } else {
            dispatch(buffer.data(), static_cast<size_t>(n));
        }
    }

    // Flush any packets whose simulated delay has expired.
    // Full scan (not just front) so out-of-order dispatchAt entries
    // caused by mid-flight latency slider changes are not stuck indefinitely.
    auto now = std::chrono::steady_clock::now();
    for (auto it = m_receiveDelayQueue.begin(); it != m_receiveDelayQueue.end(); )
    {
        if (it->dispatchAt <= now) {
            dispatch(it->data.data(), it->data.size());
            it = m_receiveDelayQueue.erase(it);
        } else {
            ++it;
        }
    }
}

void UDPClient::dispatch(const uint8_t* data, size_t n)
{
    auto pkt = decodePacket(data, n);
    if (!pkt) {
        std::cerr << "[Network] Failed to decode packet of " << n << " bytes. First byte: " << (n > 0 ? (int)data[0] : -1) << "\n";
        return;
    }
    if (pkt->type == PacketType::NET_ACCEPT) {
        std::cout << "[Network] Successfully decoded NET_ACCEPT packet\n";
    }

    auto now = std::chrono::steady_clock::now();
    auto result = reliabilityIngest(recvRel, std::move(pkt), now);

    if (result.nack) {
        NetReliableNack nack;
        nack.fromSeq = result.nack->first;
        nack.toSeq   = result.nack->second;
        auto bytes = encodePacket(nack);
        sendRawBytes(bytes);
    }

    for (auto& ready : result.ready) {
        if (ready->type == PacketType::RELIABLE_NACK) {
            auto& nack = static_cast<NetReliableNack&>(*ready);
            auto bytesList = reliabilityOnNack(sendRel, nack.fromSeq, nack.toSeq);
            for (const auto* b : bytesList) sendRawBytes(*b);
            continue;
        }
        if (onPacket) onPacket({ std::move(ready) });
    }
}

void UDPClient::reliabilityKeepalive() {
    auto now = std::chrono::steady_clock::now();
    if (reliabilityShouldKeepalive(recvRel, now)) {
        NetReliableNack nack;
        nack.fromSeq = recvRel.expectedSeq;
        nack.toSeq   = recvRel.expectedSeq;
        auto bytes = encodePacket(nack);
        sendRawBytes(bytes);
    }
}

