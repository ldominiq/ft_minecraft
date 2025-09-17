#include "Server.hpp"

Server::Server() {
    createSocket();
    fillServerInfo();
    bindSocket();
}

Server::~Server() {
    close(sockfd);
}

void Server::run(std::optional<int> &seed) {
    std::cout << "Server running on port " << PORT << "..." << std::endl;

	if (seed.has_value())
		world = std::make_unique<World>(seed.value());
	else
		world = std::make_unique<World>();

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
			ssize_t n = recvfrom(sockfd, buffer, MAXLINE, 0, (sockaddr*)&cliaddr, &addrLen);
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

		tick++; //assumes it will wrap around. meaning INT32_MAX + 1 = INT32_MIN
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

        case PacketType::NET_DISCONNECT: {
			auto& p = static_cast<NetDisconnect&>(*pkt);
			receiveDisconnect(p, cliaddr);
            break;
		}

        case PacketType::PLAYER_INPUT: {
            auto& p = static_cast<NetPlayerInputs&>(*pkt);
            receivePlayerInputs(p, cliaddr);
            break;
		}

		case PacketType::PLAYER_MOUSE_INPUT: {
			auto& p = static_cast<NetPlayerMouseInputs&>(*pkt);
			receivePlayerMouseInputs(p, cliaddr);
			break;
		}

		case PacketType::NET_MESSAGE: {
			auto& p = static_cast<NetMessage&>(*pkt);
			receiveMessage(p, cliaddr);
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

void Server::receiveDisconnect(NetDisconnect &pkt, const sockaddr_in &cliaddr)
{
	auto player = NetUtils::findPlayerByAddr(players, cliaddr);
	if (player == players.end())
		return ;

	players.erase(player);
}

void Server::receivePlayerInputs(NetPlayerInputs &pkt, const sockaddr_in &cliaddr)
{
	auto player = NetUtils::findPlayerByAddr(players, cliaddr);
	if (player == players.end())
		return ;

	player->lastPktRecvTick = currTick;
	player->setLastInputPacketReceived(pkt);
	player->loadRadius = pkt.loadRadius;
	player->setYawAndPitch(pkt.yaw, pkt.pitch);
	player->updateCameraVectors();	//order is vital. updateCameraVectors uses pkt.
}

void Server::receivePlayerMouseInputs(NetPlayerMouseInputs &pkt, const sockaddr_in &cliaddr)
{
	auto player = NetUtils::findPlayerByAddr(players, cliaddr);
	if (player == players.end())
		return ;

	player->lastPktRecvTick = currTick;
	world->processPlayerMouseInputs(*player, pkt);
}

void Server::receiveMessage(NetMessage &pkt, const sockaddr_in &cliaddr)
{
	static const std::unordered_map<std::string, GAMEMODES> gamemodeMap = {
		{"spectator", GAMEMODES::SPECTATOR},
		{"survival",  GAMEMODES::SURVIVAL},
	};

	if (pkt.message.starts_with("/"))
	{
		pkt.message.erase(0, 1); // strip leading '/'

		if (pkt.message.starts_with("gamemode "))
		{
			std::string mode = pkt.message.substr(strlen("gamemode "));

			if (auto itMode = gamemodeMap.find(mode); itMode != gamemodeMap.end())
			{
				auto player = NetUtils::findPlayerByAddr(players, cliaddr);
				player->setGamemode(itMode->second);
			}
		}
	}
	else
		messages.push_back(pkt.message);
}

// TODO : Multythread
void Server::sendAll()
{
	world->amountOfChunksSentThisTick = 0;
	for (CPlayerInfo &p : players)
	{
		p.calculateNewPosition(*world);
		world->updateVisibleChunks(p);

		sendChunk(p);
		sendPositionDeltas(p); //not deltas for now
		sendImGuiData(p);
		sendNewlyUpdatedBlocks(p);
		sendMessage(p);
		//hit/dmg ..
	}
	world->updatedBlocks.clear();
	if (!messages.empty())
		messages.pop_front();
}

void Server::sendImGuiData(CPlayerInfo &player) {
    NetImGui pkt;
	float wx = player.getPosition().x;
	float wz = player.getPosition().z;
	TerrainGenerationParams params = world->getTerrainParams();
    pkt.currentBiome = static_cast<uint8_t>(ChunkGeneration::computeBiome(params, wx, wz, ChunkGeneration::computeTerrainHeight(params, wx, wz)));
    sendPacketTo(pkt, player.addr);
}

void Server::sendChunk(CPlayerInfo &player) {

	std::vector<ChunkPos> readyChunks;
	readyChunks.swap(player.rdyChunks);

    for (auto& chunkPos : readyChunks) {
        ChunkGeneration& chunk = *world->getChunk(chunkPos.first, chunkPos.second);

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
}

// TODO : delta compression
void Server::sendPositionDeltas(CPlayerInfo &player)
{
	player.lastPositionSent = player.getPosition();
	NetPlayerMove pkt;
	pkt.snapshotTick = tick;
	pkt.inputRecvTick = player.getTick();

	pkt.positionX = player.getPosition().x;
	pkt.positionY = player.getPosition().y;
	pkt.positionZ = player.getPosition().z;

	pkt.velocityX = player.getVelocity().x;
	pkt.velocityZ = player.getVelocity().z;

	pkt.verticalVelocity = player.getVerticalVelocity();

	sendPacketTo(pkt, player.addr);
}

void Server::sendNewlyUpdatedBlocks(CPlayerInfo &player)
{
	for (auto &block : world->updatedBlocks)
	{
		NetModifiedBlockData pkt;
		pkt.x = block.first.x;
		pkt.y = block.first.y;
		pkt.z = block.first.z;
		pkt.blockType = static_cast<uint8_t>(block.second);

		sendPacketTo(pkt, player.addr);
	}
}

void Server::sendMessage(CPlayerInfo &player)
{
	if (messages.empty()) return ;
	NetMessage pkt;
	pkt.message = messages.front();
	sendPacketTo(pkt, player.addr);
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