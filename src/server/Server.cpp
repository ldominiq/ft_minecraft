#include "Server.hpp"

#include "Creeper.hpp"
Server::Server() {
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        exit(EXIT_FAILURE);
    }
#endif
    createSocket();
    fillServerInfo();
    bindSocket();
}

Server::~Server() {
    close(sockfd);
#ifdef _WIN32
    WSACleanup();
#endif
}

void Server::run(std::optional<int> &seed) {
    std::cout << "Server running on port " << PORT << "..." << std::endl;

	if (seed.has_value())
		world = std::make_unique<World>(seed.value());
	else
		world = std::make_unique<World>();

	glm::vec3 startingPos = glm::vec3(0,200, 0);
	std::shared_ptr<Creeper> crep = std::make_shared<Creeper>(startingPos);
	std::shared_ptr<Creeper> crep2 = std::make_shared<Creeper>(glm::vec3(0,90,0));
	world->livingEntities.push_back(crep);
	world->livingEntities.push_back(crep2);

	running = true;

    loop();
}

void Server::createSocket() {
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
			int n = recvfrom(sockfd, reinterpret_cast<char*>(buffer), MAXLINE, 0, (sockaddr*)&cliaddr, &addrLen);
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

		case PacketType::NET_INVENTORY_ACTION: {
			auto& p = static_cast<NetInventoryAction&>(*pkt);
			receiveInventoryAction(p, cliaddr);
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
	world->updateEntitiesPosition(players, tick);
	if (world->liquidsManager.tickSinceLastUpdate < tick - 5)
	{
		world->liquidsManager.tickSinceLastUpdate = tick;
		world->updateLiquids();
	}

	if (tick % (static_cast<int>(TPS) * 3) == 0)
		world->updateRegionStreaming(players);
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
	p.computeSpawnPosition(world->getTerrainParams());

	players.push_back(p);
	world->livingEntities.push_back(p.movement);
	world->updateRegionStreaming(players);
	
	sendAccept(cliaddr);
}

void Server::receiveDisconnect(NetDisconnect &pkt, const sockaddr_in &cliaddr)
{
	auto player = NetUtils::findPlayerByAddr(players, cliaddr);
	if (player == players.end())
		return ;

	const auto &ent = std::find(world->livingEntities.begin(), world->livingEntities.end(), player->movement);
	if (ent == world->livingEntities.end())
		return ;

	for (CPlayerInfo &p : players)
	{
		if (player->movement == p.movement) continue;

		NetEntityMove pkt;

		pkt.eEntityType = ent->get()->getEntityType();
		pkt.entityID = ent->get()->getID();
		pkt.type = -1;

		pkt.positionX = ent->get()->getPosition().x;
		pkt.positionY = ent->get()->getPosition().y;
		pkt.positionZ = ent->get()->getPosition().z;

		pkt.yaw = ent->get()->yaw;

		sendPacketTo(pkt, p.addr);
	}

	world->PlayerKnownChunks[player->id].clear();
	world->livingEntities.erase(ent);
	players.erase(player);
}

void Server::receivePlayerInputs(NetPlayerInputs &pkt, const sockaddr_in &cliaddr)
{
	auto player = NetUtils::findPlayerByAddr(players, cliaddr);
	if (player == players.end())
		return ;

 // Discard outdated or duplicate packets
	if (pkt.serverClientReconciliationTick <= player->serverClientReconciliationTick)
		return;

	if (pkt.activeHotbarSlot != (uint8_t)-1)
		player->movement->inventory.activeHotbarSlot = pkt.activeHotbarSlot;

	if (pkt.keys & IN_DROP)
	{
		ItemType type = player->movement->inventory.getItemAtSlot(player->movement->inventory.activeHotbarSlot);
		if (player->movement->inventory.removeItemsFromSlot(player->movement->inventory.activeHotbarSlot, 1))
		{
			//instead of player->movement->getEntityHeight() * 0.6f should be some hand/waist height
			glm::vec3 itemPos = player->movement->getPosition() + glm::vec3(0, player->movement->getEntityHeight() * 0.6f, 0) + player->movement->getCameraDir() * 0.2f;

			world->itemEntities.push_back(std::make_shared<ItemEntity>(itemPos, player->movement->getYaw(), type, tick, true));

			NetInventory dropItem;
			int slot = player->movement->inventory.activeHotbarSlot;
			dropItem.type = std::visit([](auto& value) -> ItemID {
				return static_cast<ItemID>(value);
			}, type);
			dropItem.amount = player->movement->inventory.getSlot(slot).second;
			dropItem.slot = slot;
			sendPacketTo(dropItem, cliaddr);
		}
	}

	if (pkt.yaw != player->movement->yaw) player->movement->positionUpdated = true;

	player->serverClientReconciliationTick = pkt.serverClientReconciliationTick;
	player->movement->setLastInputPacketReceived(pkt);

	// Queue this input for physics processing.  The queue is drained in
	// calculateNewPosition (one physics step per entry), so when the client
	// sends several inputs in rapid succession (low-FPS catch-up) the server
	// runs the matching number of physics steps instead of just one.
	constexpr int kMaxQueuedInputs = 20;
	if (static_cast<int>(player->movement->pendingInputs.size()) < kMaxQueuedInputs)
		player->movement->pendingInputs.push_back(pkt);

	if (pkt.loadRadius > 32)
		pkt.loadRadius = 32;
	else if (pkt.loadRadius < 4)
		pkt.loadRadius = 4;
	player->movement->loadRadius = pkt.loadRadius;
	player->movement->setYawAndPitch(pkt.yaw, pkt.pitch);
	player->movement->updateCameraVectors();	//order is vital. updateCameraVectors uses pkt.
}

void Server::receivePlayerMouseInputs(NetPlayerMouseInputs &pkt, const sockaddr_in &cliaddr)
{
	auto player = NetUtils::findPlayerByAddr(players, cliaddr);
	if (player == players.end())
		return ;

	if (world->processPlayerMouseInputs(*player, pkt, tick))
	{
		sendInventorySlot(player->movement->inventory.activeHotbarSlot, cliaddr);
	}
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
				player->movement->setGamemode(itMode->second);

				glm::vec3 vel = player->movement->getVelocity();
				player->movement->setVelocity(glm::vec3(vel.x, 0.0f, vel.z));
				player->movement->accumulatedFallDistance = 0.0f;

				NetPlayerGameMode pkt;
				pkt.gamemode = static_cast<uint8_t>(itMode->second);
				sendPacketTo(pkt, cliaddr);
			}
		}
	}
	else
		messages.push_back(pkt.message);
}

void Server::sendInventorySlot(int slot, const sockaddr_in &cliaddr)
{
	auto player = NetUtils::findPlayerByAddr(players, cliaddr);
	if (player == players.end())
		return;

	Inventory inv = player->movement->inventory;
	ItemID itemIDAtSlot = inv.getItemIDAtSlot(slot);
	itemStackSize_t amountAtSlot = inv.getSlot(slot).second;

	NetInventory pkt;
	pkt.type = itemIDAtSlot;
	pkt.amount = amountAtSlot;
	pkt.slot = slot;
	sendPacketTo(pkt, cliaddr);
}

void Server::receiveInventoryAction(NetInventoryAction &pkt, const sockaddr_in &cliaddr)
{
	auto player = NetUtils::findPlayerByAddr(players, cliaddr);
	if (player == players.end())
		return;

	if (pkt.slot > HAND_ID) return;
	int slot = pkt.slot;

	Inventory &inv = player->movement->inventory;

	ItemType typeAtSlot = inv.getItemAtSlot(slot);
	ItemType typeAtHand = inv.getHand().first;

	int amountAtSlot = inv.getSlot(slot).second;
	int amountAtHand = inv.getHand().second;

	if (pkt.actionType == InventoryActionType::INV_LEFT_CLICK)
	{
		if (amountAtHand == 0 || typeAtSlot != typeAtHand)
			inv.swapSlots(slot, HAND_ID);
		else
		{
			inv.mergeSlot(HAND_ID, slot);
		}
	} else if (pkt.actionType == InventoryActionType::INV_RIGHT_CLICK)
	{
		if (amountAtHand == 0)
			inv.takeHalf(slot);
		else
		{
			if (amountAtSlot == 0 || typeAtSlot == typeAtHand)
				inv.takeOneItemFromSlot(HAND_ID, std::optional<int>(slot));
			else
				inv.swapSlots(slot, HAND_ID);
		}
	}

	sendInventorySlot(slot, cliaddr);
	sendInventorySlot(HAND_ID, cliaddr);
}

// TODO : Multithread
void Server::sendAll()
{
	world->amountOfChunksSentThisTick = 0;
	sendDeaths();
	world->updateRdyChunks();
	for (CPlayerInfo &p : players)
	{
		world->updateVisibleChunks(p);
		world->updatePlayerRdyChunks(p);

		sendChunk(p);
		sendPositionDeltas(p); //not deltas for now
		sendImGuiData(p);
		sendNewlyUpdatedBlocks(p);
		sendMessage(p);
		//hit/dmg ..
	}
	sendEntitiesPositionDeltas();
	world->updatedBlocks.clear();
	if (!messages.empty())
		messages.pop_front();
	
	world->rdyChunks.clear();
}

void Server::sendDeaths()
{
	for (auto le = world->livingEntities.begin(); le != world->livingEntities.end(); le++)
	{
		if (le->get()->health <= 0)
		{
			le->get()->onDeath();
			messages.push_back("Someone has died miserably");

			if (le->get()->getLivingEntityType() != PLAYER)
			{
				NetEntityMove pkt;

				pkt.eEntityType = le->get()->getEntityType();
				pkt.entityID = le->get()->getID();
				pkt.type = -1;

				pkt.positionX = le->get()->getPosition().x;
				pkt.positionY = le->get()->getPosition().y;
				pkt.positionZ = le->get()->getPosition().z;

				pkt.yaw = le->get()->yaw;

				le = world->livingEntities.erase(le);

				for (auto player : players)
					sendPacketTo(pkt, player.addr);
			}
		}
	}
}

void Server::sendImGuiData(CPlayerInfo &player) {
    NetImGui pkt;
	float wx = player.movement->getPosition().x;
	float wz = player.movement->getPosition().z;
	TerrainGenerationParams params = world->getTerrainParams();
    pkt.currentBiome = static_cast<uint8_t>(ChunkGeneration::computeBiome(params, wx, wz, ChunkGeneration::computeTerrainHeight(params, wx, wz)));
    sendPacketTo(pkt, player.addr);
}

void Server::sendChunk(CPlayerInfo &player)
{
	std::vector<ChunkPos> rdyChunk;
	rdyChunk.swap(player.rdyChunks);

    for (auto& chunkPos : rdyChunk) {
        std::shared_ptr<ChunkGeneration> chunkG = world->getChunk(chunkPos.first, chunkPos.second);

		if (!chunkG)
		{
			std::cout << "ERROR: Chunk not found\n";
			continue ;
		}

		ChunkGeneration& chunk = *chunkG;

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
        // Account for packet encoding overhead: 1 (type) + 2 (seq) + 1 (flags) + 4 (X) + 4 (Z) + 4 (data len) = 16 bytes
        size_t payloadCapacity = MAXLINE - 16;
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
	NetPlayerMove pkt;

 pkt.serverClientReconciliationTick = player.movement->getLastAppliedServerClientReconciliationTick();

	pkt.positionX = player.movement->getPosition().x;
	pkt.positionY = player.movement->getPosition().y;
	pkt.positionZ = player.movement->getPosition().z;

	pkt.velocityX = player.movement->getVelocity().x;
	pkt.velocityY = player.movement->getVelocity().y;
	pkt.velocityZ = player.movement->getVelocity().z;

	pkt.yaw = player.movement->yaw;
	pkt.pitch = player.movement->pitch;

	pkt.health = player.movement->health;
	pkt.slipperinessPrev = player.movement->getSlipperinessPrev();
	pkt.accumulatedFallDistance = player.movement->getAccumulatedFallDistance();
	pkt.onGround = player.movement->isOnGround() ? 1 : 0;
	pkt.jumpBoostApplied = player.movement->getJumpBoostApplied() ? 1 : 0;

	//std::cout << "tick: " << pkt.serverClientReconciliationTick << "\n" <<
	//"pos: (" << pkt.positionX << ", " << pkt.positionY << ", " << pkt.positionZ << ")\n" <<
	//"vel: (" << pkt.velocityX << ", " << pkt.velocityY << ", " << pkt.velocityZ << ")\n" <<
	//"splitPrev: (" << player.movement->getSlipperinessPrev() << ")\n" <<
	//"onGround: (" << player.movement->isOnGround() << ")\n" <<
	//"fallDistance: (" << player.movement->getAccumulatedFallDistance() << ")\n" <<
	//"jumpBoost: (" << player.movement->getJumpBoostApplied() << ")\n";
	//std::cout << "------------------\n\n";

	sendPacketTo(pkt, player.addr);
}

// TODO : delta compression AND refactor this sh*t (put in a snapshot and send multiple at once or something) AND only send if the item moved
void Server::sendEntitiesPositionDeltas()
{
	//gotta exclude current player
	for (auto &entity : world->livingEntities)
	{
		for (CPlayerInfo &p : players)
		{
			if (entity == p.movement || !entity->positionUpdated) continue;

			NetEntityMove pkt;

			pkt.eEntityType = entity->getEntityType();
			pkt.entityID = entity->getID();
			pkt.type = static_cast<LivingEntityType>(entity->getLivingEntityType());

			pkt.positionX = entity->getPosition().x;
			pkt.positionY = entity->getPosition().y;
			pkt.positionZ = entity->getPosition().z;

			pkt.yaw = entity->yaw;

			sendPacketTo(pkt, p.addr);
		}

		entity->positionUpdated = false;
	}

	for (auto &entity : world->itemEntities)
	{
		for (CPlayerInfo &p : players)
		{
			if (!entity->positionUpdated) continue;

			NetEntityMove pkt;

			pkt.eEntityType = entity->getEntityType();
			pkt.entityID = entity->getID();

			pkt.type = entity->getItemID();

			pkt.positionX = entity->getPosition().x;
			pkt.positionY = entity->getPosition().y;
			pkt.positionZ = entity->getPosition().z;

			pkt.yaw = entity->yaw;

			sendPacketTo(pkt, p.addr);
		}

		entity->positionUpdated = false;
	}

	//todo maybe change this to somehow group the packets to not send so many of them
	for (auto &deletedEntityPkt : world->deletedEntitiesPkts)
	{
		for (CPlayerInfo &p : players)
			sendPacketTo(deletedEntityPkt, p.addr);
	}
	for (auto &pickedUpItems : world->pickedUpItems)
		sendPacketTo(pickedUpItems.second, pickedUpItems.first);

	world->deletedEntitiesPkts.clear();
	world->pickedUpItems.clear();
}

void Server::sendNewlyUpdatedBlocks(CPlayerInfo &player)
{
	std::vector<PacketPtr> modifiedBlocks;

	for (auto &block : world->updatedBlocks)
	{
		auto pkt = std::make_unique<NetModifiedBlockData>();
		pkt->x = block.first.x;
		pkt->y = block.first.y;
		pkt->z = block.first.z;
		pkt->blockType = static_cast<ItemID>(block.second);

		modifiedBlocks.push_back(std::move(pkt));
	}
	sendNewGroupPacketTo(modifiedBlocks, player.addr);
}

void Server::sendMessage(CPlayerInfo &player)
{
	if (messages.empty()) return ;
	NetMessage pkt;
	pkt.message = messages.front();
	sendPacketTo(pkt, player.addr);
}

void Server::sendNewGroupPacketTo(std::vector<PacketPtr>& pkts, const sockaddr_in& cliaddr)
{
    NetPacketGroup group;
    int currSize = 6; // 6 bytes because technically it's 2 bytes of group packet u16 "count" + 4bytes of outer layer header (u8+u16+u8). probably.

    auto it = pkts.begin();
    while (it != pkts.end()) {
        auto buf = encodePacket(**it);
        int nextSize = currSize + 4 + buf.size(); // 4 bytes for per-packet size header

        // if next packet would exceed max size -> send current group first
        if (nextSize > MAXLINE) {
            if (!group.rawPackets.empty()) {
                sendPacketTo(group, cliaddr);
                group = NetPacketGroup();
                currSize = 6; // reset
            }
            continue; // retry current packet
        }

        group.rawPackets.push_back(std::move(buf)); // use rawPackets directly
        currSize = nextSize;
        it = pkts.erase(it);
    }

    if (!group.rawPackets.empty()) {
        sendPacketTo(group, cliaddr);
    }
}

void Server::sendPacketTo(const Packet& pkt, const sockaddr_in &cliaddr) {
	auto bytes = encodePacket(pkt);
	sendto(sockfd, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0, (sockaddr*)&cliaddr, sizeof(cliaddr));
}

void Server::sendAccept(const sockaddr_in &cliaddr)
{
	std::vector<PacketPtr> groupPkt;

	auto player = NetUtils::findPlayerByAddr(players, cliaddr);
	if (player != players.end())
	{		
		//gotta exclude current player
		for (auto &entity : world->livingEntities)
		{
			if (entity == player->movement) continue;

			auto pkt = std::make_unique<NetEntityMove>();

			pkt->eEntityType = entity->getEntityType();
			pkt->entityID = entity->getID();
			pkt->type = static_cast<LivingEntityType>(entity->getLivingEntityType());

			pkt->positionX = entity->getPosition().x;
			pkt->positionY = entity->getPosition().y;
			pkt->positionZ = entity->getPosition().z;

			pkt->yaw = entity->yaw;

			groupPkt.push_back(std::move(pkt));
		}

		for (auto &entity : world->itemEntities)
		{
			auto pkt = std::make_unique<NetEntityMove>();

			pkt->eEntityType = entity->getEntityType();
			pkt->entityID = entity->getID();
			pkt->type = static_cast<ItemID>(entity->getItemID());

			pkt->positionX = entity->getPosition().x;
			pkt->positionY = entity->getPosition().y;
			pkt->positionZ = entity->getPosition().z;

			pkt->yaw = entity->yaw;

			groupPkt.push_back(std::move(pkt));
		}

		int twohundred0 = 200;
		int twohundred1 = 200;
		int twohundred2 = 200;
		player->movement->inventory.insertItemsToSlot(BlockType::DIRT, 0, twohundred0);
		player->movement->inventory.insertItemsToSlot(BlockType::WATER, 8, twohundred1);
		player->movement->inventory.insertItemsToSlot(BlockType::STONE, 1, twohundred2);

		auto pkt1 = std::make_unique<NetInventory>();
		pkt1->amount = 200;
		pkt1->slot = 0;
		pkt1->type = static_cast<std::underlying_type_t<BlockType>>(BlockType::DIRT);

		auto pkt2 = std::make_unique<NetInventory>();
		pkt2->amount = 200;
		pkt2->slot = 8;
		pkt2->type = static_cast<std::underlying_type_t<BlockType>>(BlockType::WATER);

		auto pkt3 = std::make_unique<NetInventory>();
		pkt3->amount = 200;
		pkt3->slot = 1;
		pkt3->type = static_cast<std::underlying_type_t<BlockType>>(BlockType::STONE);

		groupPkt.push_back(std::move(pkt1));
		groupPkt.push_back(std::move(pkt2));
		groupPkt.push_back(std::move(pkt3));
	}

	sendNewGroupPacketTo(groupPkt, cliaddr);

	NetAccept acceptPkt;
	sendPacketTo(acceptPkt, cliaddr);
}

void Server::saveWorldOnExit()
{
	world->saveRegionsOnExit();
}