#include "Server.hpp"

Server::Server() {
    createSocket();
    fillServerInfo();
    bindSocket();
}

Server::~Server() {
    close(sockfd);
}

void Server::run() {
    std::cout << "Server running on port " << PORT << "..." << std::endl;

	world = std::make_unique<World>(); //No seed for now;

	running = true;

    loop();
}

void Server::createSocket() {
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
}

void Server::fillServerInfo() {
    memset(&servaddr, 0, sizeof(servaddr));

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = INADDR_ANY;
    servaddr.sin_port = htons(PORT);
}

void Server::bindSocket() {
    if (bind(sockfd, (const struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }
}

void Server::loop() {
	sockaddr_in cliaddr;
	socklen_t addrLen = sizeof(cliaddr);

	auto nextTick = std::chrono::steady_clock::now();
	auto lastTick = nextTick;

    while (running)
	{
		currTick = std::chrono::steady_clock::now();
		
		// 1. Poll sockets (non-blocking)
		while (true) {
			ssize_t n = recvfrom(sockfd, buffer, MAXLINE, 0, (sockaddr*)&cliaddr, &len);
			if (n < 0) {
				if (errno == EWOULDBLOCK || errno == EAGAIN) break; // no more packets
				perror("recvfrom error");
				break;
			}
			dispatch(buffer, n, cliaddr);
		}

	    deltaTime = std::chrono::duration<float>(currTick - lastTick).count();
        lastTick = currTick;

		// 2. Run game tick logic
		gameTick();

		nextTick += TICK_RATE;
		std::this_thread::sleep_until(nextTick);
        if (std::chrono::steady_clock::now() > nextTick + TICK_RATE) {
            std::cerr << "⚠️ Server tick lagging behind!\n";
            nextTick = std::chrono::steady_clock::now(); // resync
        }
	}
}

void Server::dispatch(const uint8_t *data, int n, sockaddr_in &cliaddr)
{
    auto pkt = decodePacket(data, n); // now returns unique_ptr<Packet>
    switch (pkt->type) {
        case PacketType::NET_CONNECT: {
			auto& p = static_cast<NetConnect&>(*pkt);
			receiveConnect(p, cliaddr);
            break;
		}

        case PacketType::PLAYER_INPUT: {
            auto& p = static_cast<NetPlayerInputs&>(*pkt);
            receivePlayerInputs(p, cliaddr);
            break;
		}

        default:
            std::cout << "Unknown packet type! id=" << (int)pkt->type << "\n";
            break;
    }
}

void Server::gameTick()
{
	sendAll();
}

void Server::receiveConnect(NetConnect &pkt, const sockaddr_in &cliaddr)
{
	if (players.size() >= MAX_CLIENTS) return ;

	std::cout << "New client connected!\n";

    CPlayerInfo p; //deserializePlayerInfo(pkt.payload);
	p.id = players.size();
	p.addr = cliaddr;
	p.connected = true;

	players.push_back(p);
	
	sendAccept(cliaddr);
}

void Server::receivePlayerInputs(NetPlayerInputs &pkt, const sockaddr_in &cliaddr)
{
	auto player = NetUtils::findPlayerByAddr(players, cliaddr);
	if (!player)
		return ;

	player->lastPktRecvTick = currTick;
	player->updatePosition(pkt, deltaTime);
}

void Server::sendAll()
{
	for (CPlayerInfo &p : players)
	{
		world->updateVisibleChunks(p);
		sendChunk(p);
		sendPositionDeltas(p); //not deltas for now
		//send mobs position
		//send player position
		//hit/dmg ..
	}
}

void Server::sendChunk(CPlayerInfo &player) {
    for (auto& chunkPos : player.rdyChunks) {
        Chunk& chunk = *world->getChunk(chunkPos.first, chunkPos.second);

        // 1. Serialize chunk into memory
        std::ostringstream oss(std::ios::binary);
        chunk.saveToStream(oss);
        std::string chunkData = oss.str();

        // 2. Compress with ZSTD
        size_t maxCompressedSize = ZSTD_compressBound(chunkData.size());
        std::vector<uint8_t> compressed(maxCompressedSize);
        size_t compressedSize = ZSTD_compress(compressed.data(), maxCompressedSize,
                                              chunkData.data(), chunkData.size(), 1);
        if (ZSTD_isError(compressedSize)) {
            std::cerr << "Compression failed!\n";
            continue;
        }
        compressed.resize(compressedSize); // shrink to actual size

		NetChunkHeader CH;
		CH.X = chunkPos.first;
		CH.Z = chunkPos.second;
		CH.compressedSize = static_cast<uint32_t>(compressed.size());
		CH.uncompressedSize = static_cast<uint32_t>(chunkData.size());
		CH.flags = PacketFlags::Compressed;
		sendPacketTo(CH, player.addr);

        // 3. Split into packets , not really needed for now as data will be smaller than MAXLINE but oh well!
        size_t payloadCapacity = MAXLINE;
        uint16_t sequence = 0;

        for (size_t offset = 0; offset < compressed.size(); offset += payloadCapacity) {
            size_t chunkSize = std::min(payloadCapacity, compressed.size() - offset);

            NetChunkData CD;
			CD.X = chunkPos.first;
			CD.Z = chunkPos.second;
			CD.sequence = sequence++;
			CD.data.assign(compressed.begin() + offset, compressed.begin() + offset + chunkSize);
			// CD.data.assign(compressed.begin(), compressed.end());

			if (compressed.size() - offset <= payloadCapacity)
            	CD.flags = PacketFlags::FinalChunk; // could mark as compressed & vital

            sendPacketTo(CD, player.addr);
        }
    }
    player.rdyChunks.clear(); // sent all
}

// TODO : delta compression
void Server::sendPositionDeltas(CPlayerInfo &player)
{
	player.lastPositionSent = player.getPosition();
	NetPlayerMove pkt;
	pkt.positionX = player.getPosition().x;
	pkt.positionY = player.getPosition().y;
	pkt.positionZ = player.getPosition().z;
	sendPacketTo(pkt, player.addr);
	// std::cout << "sending positions: " << pkt.positionX << " " << pkt.positionY << " " << pkt.positionZ << std::endl;
}

void Server::sendPacketTo(const Packet& pkt, const sockaddr_in &cliaddr) {
	auto bytes = encodePacket(pkt);
	sendto(sockfd, bytes.data(), bytes.size(), 0, (sockaddr*)&cliaddr, sizeof(cliaddr));
}

void Server::sendAccept(const sockaddr_in &cliaddr)
{
	NetAccept acceptPkt;
	sendPacketTo(acceptPkt, cliaddr);
}

void Server::saveWorldOnExit()
{
	world->saveRegionsOnExit();
}