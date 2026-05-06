#include "Server.hpp"

#include "Creeper.hpp"
#include "Zombie.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <sstream>

static std::mt19937 spawnRng(std::random_device{}());
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

	// Stop the ping thread
	{
		std::lock_guard<std::mutex> lock(pingMutex);
		running = false;
	}
	pingCV.notify_one();
	if (pingThread.joinable()) {
		// Blocks the current thread until the thread identified by *this finishes its execution. 
		pingThread.join();
	}

	{
		std::lock_guard<std::mutex> lock(dumpThreadsMutex);
		for (auto& thread : dumpThreads) {
			if (thread.joinable()) {
				thread.join();
			}
		}
	}
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

	running = true;

	// Start the ping processing thread
	pingThread = std::thread(&Server::pingLoop, this);

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
#ifdef _WIN32
        std::cerr << "bind failed: " << WSAGetLastError() << "\n";
#else
        perror("bind failed");
#endif
        exit(EXIT_FAILURE);
    }
}

void Server::loop() {
	sockaddr_in cliaddr{};
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
			if (n > 0 && buffer[0] == static_cast<uint8_t>(PacketType::NET_PING)) {
				try {
					auto pkt = decodePacket(buffer, n);
					auto& ping = static_cast<NetPing&>(*pkt);
					// send data to the worker thread
					{
						std::lock_guard<std::mutex> lock(pingMutex);
						pingQueue.push({ cliaddr, ping.timestamp });
					}
					pingCV.notify_one();
				}
				catch (const std::exception& e) {
					std::cerr << "[Server] Failed to decode ping packet: " << e.what() << "\n";
				}
			}
			else {
				dispatch(buffer, n, cliaddr);
			}
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

void Server::dispatchPacket(PacketPtr &pkt, sockaddr_in &cliaddr)
{
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

		case PacketType::GROUP: {
			auto& group = static_cast<NetPacketGroup&>(*pkt);
			for (auto& inner : group.unpack())
				dispatchPacket(inner, cliaddr);
			break;
		}

		case PacketType::NET_SKY_TIME: {
			// Only allow if player is connected
			if (NetUtils::findPlayerByAddr(players, cliaddr) == players.end())
				break;
			auto& p   = static_cast<NetSkyTime&>(*pkt);
			if (p.skyMode > 1) // validate mode
				break;
			p.skyTimeSpeed = std::clamp(p.skyTimeSpeed, 0.001f, 10.0f); // validate speed
			updateSkyTime(p);
			broadcastSkyTime();
			break;
		}

		case PacketType::NET_TERRAIN_PARAMS: {
			auto& p = static_cast<NetTerrainParams&>(*pkt);
			receiveTerrainParams(p, cliaddr);
			break;
		}

		// NET_PING is handled by pingThread before dispatch() is called

		case PacketType::NET_PLAYER_PING: {
			auto& p = static_cast<NetPlayerPing&>(*pkt);
			if (!std::isfinite(p.pingMs) || p.pingMs < 0.0f || p.pingMs > 9999.0f)
				break; // discard invalide ping
			auto player = NetUtils::findPlayerByAddr(players, cliaddr);
			if (player != players.end())
				player->pingMs = p.pingMs;
			break;
		}

        default:
            std::cout << "Unknown packet type! id=" << (int)pkt->type << "\n";
            break;

	}
}

void Server::dispatch(const uint8_t *data, int n, sockaddr_in &cliaddr)
{
    PacketPtr pkt;
    try {
        pkt = decodePacket(data, n);
    } catch (const std::exception& e) {
        std::cerr << "[Network] decode failed: " << e.what() << "\n";
        return;
    }
    if (!pkt) return;

    // Unknown peer: only NET_CONNECT is allowed. Dispatch it, then bootstrap the
    // receiver's expectedSeq past this packet (its seq=0 is already consumed).
    auto player = NetUtils::findPlayerByAddr(players, cliaddr);
    if (player == players.end()) {
        if (pkt->type == PacketType::NET_CONNECT) {
            dispatchPacket(pkt, cliaddr);
            auto p = NetUtils::findPlayerByAddr(players, cliaddr);
            if (p != players.end()) {
                p->recvRel.expectedSeq    = 1;
                p->recvRel.lastProgressAt = currTick;
            }
        }
        return;
    }

    // Known peer: route through the reliability layer.
    auto result = reliabilityIngest(player->recvRel, std::move(pkt), currTick);
    if (result.nack) {
        NetReliableNack nack;
        nack.fromSeq = result.nack->first;
        nack.toSeq   = result.nack->second;
        sendPacketTo(nack, cliaddr);
    }
    for (auto& ready : result.ready) {
        if (ready->type == PacketType::RELIABLE_NACK) {
            auto& nack = static_cast<NetReliableNack&>(*ready);
            auto bytesList = reliabilityOnNack(player->sendRel, nack.fromSeq, nack.toSeq);
            for (const auto* b : bytesList) {
                sendRawBytesTo(*b, cliaddr);
            }
            continue;
        }
        dispatchPacket(ready, cliaddr);
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

	world->advanceSkyTime();

	// Attempt mob spawning every 5 seconds
	if (tick > 0 && tick % (static_cast<int>(TPS) * 5) == 0)
		trySpawnNightMobs();

	// Despawn mobs with no player nearby once per second.
	if (tick > 0 && tick % static_cast<int>(TPS) == 0)
		despawnDistantMobs();

	// Broadcast every 20 ticks (~1s)
	if (tick % static_cast<int>(TPS) == 0)
		broadcastSkyTime();
	if (tick % static_cast<int>(TPS * 2) == 0)
		broadcastPingList();

	reliabilityKeepalive();

	sendAll();
}

void Server::updateSkyTime(NetSkyTime &pkt) {

	world->setSkyTime({
		.skyTimeOffset 	= pkt.skyTimeOffset,
		.sunYawDeg 		= pkt.sunYawDeg,
		.sunPauseTimer 	= pkt.sunPauseTimer,
		.sunStepTimer 	= pkt.sunStepTimer,
		.sunStepping 	= pkt.sunStepping,
		.skyTimePaused 	= pkt.skyTimePaused,
		.skyMode 		= pkt.skyMode,
		.skyTimeSpeed 	= pkt.skyTimeSpeed
	});
}

void Server::broadcastSkyTime() {
	const auto& s = world->getSkyTimeState();
    NetSkyTime pkt;
    pkt.skyTimeOffset  = s.skyTimeOffset;
    pkt.sunYawDeg      = s.sunYawDeg;
    pkt.sunPauseTimer  = s.sunPauseTimer;
    pkt.sunStepTimer   = s.sunStepTimer;
    pkt.sunStepping    = s.sunStepping;
    pkt.skyTimePaused  = s.skyTimePaused;
    pkt.skyMode        = s.skyMode;
    pkt.skyTimeSpeed   = s.skyTimeSpeed;
    for (CPlayerInfo& p : players)
        sendPacketTo(pkt, p.addr);
}

void Server::broadcastPingList()
{
	NetPingList pkt;
	for (const CPlayerInfo& p : players)
		pkt.entries.push_back({ static_cast<uint32_t>(p.movement->getID()), static_cast<uint32_t>(p.id), p.pingMs });
	for (const CPlayerInfo& p : players)
		sendPacketTo(pkt, p.addr);
}

void Server::receiveConnect(NetConnect &pkt, const sockaddr_in &cliaddr)
{
	if (players.size() >= MAX_CLIENTS) return ;

	std::cout << "New client connecting from " << inet_ntoa(cliaddr.sin_addr) << ":" << ntohs(cliaddr.sin_port) << "...\n";

    CPlayerInfo p; //deserializePlayerInfo(pkt.payload);
	p.id = nextPlayerId++;
	p.addr = cliaddr;
	p.connected = true;
	p.computeSpawnPosition(world->getTerrainParams());

	auto movement = p.movement;
	players.push_back(std::move(p));
	world->livingEntities.push_back(std::move(movement));
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

		pkt.positionX = ent->get()->getPositionD().x;
		pkt.positionY = ent->get()->getPositionD().y;
		pkt.positionZ = ent->get()->getPositionD().z;

		pkt.yaw = ent->get()->yaw;
		pkt.pitch = ent->get()->pitch;

		sendPacketTo(pkt, p.addr);
	}

	// Erase rather than clear: ids are never reused, so the entry stays dead.
	world->PlayerKnownChunks.erase(player->id);
	world->livingEntities.erase(ent);
	players.erase(player);
}

void Server::receivePlayerInputs(NetPlayerInputs &pkt, const sockaddr_in &cliaddr)
{
	auto player = NetUtils::findPlayerByAddr(players, cliaddr);
	if (player == players.end())
		return;

	// Dead or waiting to respawn: ignore all movement/keyboard input.
	if (player->movement->health <= 0.0f || player->movement->pendingDeathRemovalTicks > 0)
		return;

 	// Discard outdated or duplicate packets
	if (pkt.serverClientReconciliationTick <= player->serverClientReconciliationTick)
		return;

	if (pkt.activeHotbarSlot != (uint8_t)-1)
		player->movement->inventory->activeHotbarSlot = pkt.activeHotbarSlot;

	if (pkt.keys & IN_DROP)
	{
		ItemType type = player->movement->inventory->getItemAtSlot(player->movement->inventory->activeHotbarSlot);
		if (player->movement->inventory->removeItemsFromSlot(player->movement->inventory->activeHotbarSlot, 1))
		{
			//instead of player->movement->getEntityHeight() * 0.6f should be some hand/waist height
			glm::vec3 itemPos = player->movement->getPosition() + glm::vec3(0, player->movement->getEntityHeight() * 0.6f, 0) + player->movement->getCameraDir() * 0.2f;

			world->itemEntities.push_back(std::make_shared<ItemEntity>(itemPos, player->movement->getYaw(), type, tick, true));

			NetInventory dropItem;
			int slot = player->movement->inventory->activeHotbarSlot;
			dropItem.inventoryTypeID = static_cast<uint8_t>(InventoryType::PLAYER);
			dropItem.type = std::visit([](auto& value) -> ItemID {
				return static_cast<ItemID>(value);
			}, type);
			dropItem.amount = player->movement->inventory->getSlot(slot).second;
			dropItem.slot = slot;
			sendPacketTo(dropItem, cliaddr);
		}
	}

	if (pkt.yaw != player->movement->yaw || pkt.pitch != player->movement->pitch)
    	player->movement->rotationUpdated = true;

	player->serverClientReconciliationTick = pkt.serverClientReconciliationTick;
	player->movement->setLastInputPacketReceived(pkt);

	// Queue this input for physics processing.  The queue is drained in
	// calculateNewPosition (one physics step per entry), so when the client
	// sends several inputs in rapid succession (low-FPS catch-up) the server
	// runs the matching number of physics steps instead of just one.
	constexpr int kMaxQueuedInputs = 20;
	if (static_cast<int>(player->movement->pendingInputs.size()) >= kMaxQueuedInputs)
		player->movement->pendingInputs.pop_front(); // drop oldest to keep ack/queue consistent
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

	// Dead or waiting to respawn: ignore all mouse actions.
	if (player->movement->health <= 0.0f || player->movement->pendingDeathRemovalTicks > 0)
		return;

	if (world->processPlayerMouseInputs(*player, pkt, tick))
	{
		NetInventory dropItem;
		int slot = player->movement->inventory->activeHotbarSlot;
		dropItem.inventoryTypeID = static_cast<uint8_t>(InventoryType::PLAYER);
		dropItem.type = player->movement->inventory->getActiveItemID();
		dropItem.amount = player->movement->inventory->getSlot(slot).second;
		dropItem.slot = slot;
		sendPacketTo(dropItem, cliaddr);
	}

	if (pkt.mouseButtons & (IN_LEFT_CLICK | IN_RIGHT_CLICK))
		player->movement->pendingArmSwing = true;
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
        else if (pkt.message.starts_with("dump "))
        {
            auto player = NetUtils::findPlayerByAddr(players, cliaddr);
            if (player == players.end())
                return;

            std::istringstream iss(pkt.message.substr(strlen("dump ")));
            std::string mode;
            int size = 500;
            int downsample = 16;
            iss >> mode;
            if (!(iss >> size)) size = 500;
            if (!(iss >> downsample)) downsample = 16;

            size = std::clamp(size, 1, 1024);
            downsample = std::clamp(downsample, 1, 256);

            const glm::vec3 pos = player->movement->getPosition();
            const int centerChunkX = static_cast<int>(std::floor(pos.x / Chunk::WIDTH));
            const int centerChunkZ = static_cast<int>(std::floor(pos.z / Chunk::DEPTH));

            const TerrainGenerationParams paramsCopy = world->getTerrainParams();

            if (mode == "noises") {
                messages.push_back("[server] Generating noise maps...");
				std::lock_guard<std::mutex> lock(dumpThreadsMutex);
                dumpThreads.emplace_back([this, paramsCopy, centerChunkX, centerChunkZ, size, downsample]() {
                    world->dumpHeightmap(paramsCopy, centerChunkX, centerChunkZ, size, size, downsample, 1);
                });
            } else if (mode == "hydro") {
                messages.push_back("[server] Generating hydro maps...");
				std::lock_guard<std::mutex> lock(dumpThreadsMutex);
                dumpThreads.emplace_back([this, paramsCopy, centerChunkX, centerChunkZ, size, downsample]() {
                    world->dumpHeightmap(paramsCopy, centerChunkX, centerChunkZ, size, size, downsample, 2);
                });
            } else if (mode == "heightmap") {
                messages.push_back("[server] Generating terrain heightmap...");
				std::lock_guard<std::mutex> lock(dumpThreadsMutex);
                dumpThreads.emplace_back([this, paramsCopy, centerChunkX, centerChunkZ, size, downsample]() {
                    world->dumpHeightmap(paramsCopy, centerChunkX, centerChunkZ, size, size, downsample, 0);
                });
            } else if (mode == "biome") {
                messages.push_back("[server] Generating biome map...");
				std::lock_guard<std::mutex> lock(dumpThreadsMutex);
                dumpThreads.emplace_back([this, paramsCopy, centerChunkX, centerChunkZ, size, downsample]() {
                    world->dumpBiomeMap(paramsCopy, centerChunkX, centerChunkZ, size, size, downsample);
                });
            } else {
                messages.push_back("[server] Unknown dump mode. Use: noises | hydro | heightmap | biome");
            }
        }
        else if (pkt.message.starts_with("tp "))
        {
            auto player = NetUtils::findPlayerByAddr(players, cliaddr);
            if (player == players.end()) return;

            std::istringstream iss(pkt.message.substr(strlen("tp ")));
            float x, y, z;
            if (iss >> x >> y >> z) {
                player->movement->setPosition(glm::vec3(x, y, z));
                player->movement->setVelocity(glm::vec3(0.0f));
                player->movement->accumulatedFallDistance = 0.0f;
            } else {
                messages.push_back("[server] Usage: /tp <x> <y> <z>");
            }
        }
	}
	else
		messages.push_back(pkt.message);
}

void Server::receiveTerrainParams(NetTerrainParams &pkt, const sockaddr_in &cliaddr)
{
	auto player = NetUtils::findPlayerByAddr(players, cliaddr);
	if (player == players.end()) return;

	world->setTerrainParams(pkt.toParams());

	// Broadcast the updated params to all connected clients
	for (auto& player : players) {
		sendPacketTo(pkt, player.addr);
	}

	std::cout << "[Server] Terrain parameters updated by client\n";
}

void Server::receiveInventoryAction(NetInventoryAction &pkt, const sockaddr_in &cliaddr)
{
	auto player = NetUtils::findPlayerByAddr(players, cliaddr);
	if (player == players.end())
		return;

	int slot = pkt.slot;

	std::shared_ptr<IInventory> inv;
	if (static_cast<InventoryType>(pkt.inventoryTypeID) == InventoryType::PLAYER)
		inv = player->movement->inventory;
	else if (static_cast<InventoryType>(pkt.inventoryTypeID) == InventoryType::CRAFTING_STATION)
		inv = player->movement->craftingStation;
	else
		return ;

	std::vector<PacketPtr> pktsToSend;

	//xd?
	if (pkt.modifier == InventoryModifiers::INV_DRAG_CANCEL || pkt.modifier == InventoryModifiers::INV_DRAG_END)
	{
		player->movement->inventory->hasDraggedSlots = false;
		player->movement->craftingStation->hasDraggedSlots = false;
	}

	auto sendHand = [&player, &pktsToSend]()
	{
		auto pkt = std::make_unique<NetInventory>();
		
		int handId = player->movement->inventory->getHandID();
		auto hand = player->movement->inventory->getHandPtr();
		if (!hand)
			return ;
			
		pkt->inventoryTypeID = static_cast<uint8_t>(InventoryType::PLAYER);
		pkt->type = itemTypeToItemID(hand->first);
		pkt->amount = hand->second;
		pkt->slot = handId;
		pktsToSend.push_back(std::move(pkt));
	};

	if (pkt.modifier == InventoryModifiers::INV_DRAG_CANCEL || pkt.modifier == InventoryModifiers::INV_DRAG_ADD)
	{
		auto slots = player->movement->getDraggedSlots();
		if (slots->empty()) //would be bug
			return ;

		for (auto &slot : *slots)
		{
			auto slotInv = slot.slotIndex.inventoryType;

			//reset the inventories to their original values
			if (slotInv == InventoryType::PLAYER)
				player->movement->inventory->setSlot(slot.slotIndex.slotIndex, slot.originalValue.second, slot.originalValue.first);
			else if (slotInv == InventoryType::CRAFTING_STATION)
				player->movement->craftingStation->setSlot(slot.slotIndex.slotIndex, slot.originalValue.second, slot.originalValue.first);

			int handId = player->movement->inventory->getHandID();
			if (pkt.modifier == InventoryModifiers::INV_DRAG_CANCEL || (pkt.modifier == InventoryModifiers::INV_DRAG_ADD && slot.slotIndex.slotIndex != handId))
			{
				auto pkt = std::make_unique<NetInventory>();
				pkt->inventoryTypeID = static_cast<uint8_t>(slot.slotIndex.inventoryType);
				pkt->type = itemTypeToItemID(slot.originalValue.first);
				pkt->amount = slot.originalValue.second;
				pkt->slot = slot.slotIndex.slotIndex;
				pktsToSend.push_back(std::move(pkt));
			}
		}

		player->movement->inventory->setHandPtr(player->movement->getDraggedSlots()->front().originalValue);
		if (pkt.modifier == InventoryModifiers::INV_DRAG_CANCEL)
		{
			sendHand();
			sendNewGroupPacketTo(pktsToSend, cliaddr);
			slots->clear();
			return ;
		}
	}

	if (pkt.modifier == InventoryModifiers::INV_DRAG_ADD)
	{
		//try to add the new drag slot in both inventories;
		player->movement->inventory->addDraggedSlot(pkt);
		player->movement->craftingStation->addDraggedSlot(pkt);

		if (pkt.inventoryTypeID == static_cast<uint8_t>(InventoryType::PLAYER) || player->movement->inventory->hasDraggedSlots == true)
			player->movement->inventory->handleDragModifier(pkt, pktsToSend);
		if (pkt.inventoryTypeID == static_cast<uint8_t>(InventoryType::CRAFTING_STATION) || player->movement->craftingStation->hasDraggedSlots == true)
		{
			player->movement->craftingStation->handleDragModifier(pkt, pktsToSend);
			player->movement->craftingStation->checkResult(pktsToSend);
		}

		sendHand();

		sendNewGroupPacketTo(pktsToSend, cliaddr);
		return ;
	}

	if (inv->handleInventoryAction(pkt, pktsToSend))
		sendNewGroupPacketTo(pktsToSend, cliaddr);

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

void Server::trySpawnNightMobs()
{
	// Night-time check: sun elevation is cos(skyTimeOffset * 0.1) (see Lighting.cpp).
	// Negative elevation means the sun is below the horizon — i.e. night.
	// Using this formulation avoids wrap-around issues as skyTimeOffset accumulates.
	const float skyT = world->getSkyTimeState().skyTimeOffset;
	if (std::cos(skyT * 0.1f) >= 0.0f)
		return;
	if (players.empty())
		return;

	constexpr int MAX_ZOMBIES_PER_PLAYER  = 10;
	constexpr int MAX_CREEPERS_PER_PLAYER = 50;
	constexpr int MIN_SPAWN_DIST = 40;
	constexpr int MAX_SPAWN_DIST = 80;
	constexpr int SCAN_TOP_Y = 200;
	constexpr int SCAN_BOTTOM_Y = 4;

	// Count current hostile mobs.
	int zombieCount  = 0;
	int creeperCount = 0;
	for (auto &e : world->livingEntities) {
		if (!e) continue;
		if (e->getLivingEntityType() == ZOMBIE)  zombieCount++;
		if (e->getLivingEntityType() == CREEPER) creeperCount++;
	}

	auto findGroundSpawn = [&](const glm::vec3 &ppos, glm::vec3 &out) -> bool {
		float angle = glm::radians(static_cast<float>(std::uniform_int_distribution<int>(0, 359)(spawnRng)));
		int dist = std::uniform_int_distribution<int>(MIN_SPAWN_DIST, MAX_SPAWN_DIST - 1)(spawnRng);
		int sx = static_cast<int>(std::floor(ppos.x + std::cos(angle) * dist));
		int sz = static_cast<int>(std::floor(ppos.z + std::sin(angle) * dist));

		auto isAir = [&](int y) {
			BlockType b = world->getBlockWorld({sx, y, sz});
			return b == BlockType::AIR;
		};
		auto isSpawnableGround = [&](int y) {
			BlockType b = world->getBlockWorld({sx, y, sz});
			return b != BlockType::END && isBlockSolid(b);
		};

		int groundY = -1;
		bool airAbove1 = isAir(SCAN_TOP_Y + 1);
		bool airAbove2 = isAir(SCAN_TOP_Y);
		for (int y = SCAN_TOP_Y; y >= SCAN_BOTTOM_Y; --y) {
			if (isSpawnableGround(y) && airAbove1 && airAbove2) {
				groundY = y;
				break;
			}
			airAbove2 = airAbove1;
			airAbove1 = isAir(y);
		}
		if (groundY < 0) return false;

		glm::vec3 spawnPos(sx + 0.5f, static_cast<float>(groundY + 1), sz + 0.5f);
		glm::vec3 d = spawnPos - ppos;
		if (d.x * d.x + d.z * d.z < float(MIN_SPAWN_DIST * MIN_SPAWN_DIST) * 0.25f) return false;
		out = spawnPos;
		return true;
	};

	for (auto &player : players) {
		if (!player.movement) continue;

		// Zombies: unchanged rate.
		if (zombieCount < MAX_ZOMBIES_PER_PLAYER * static_cast<int>(players.size())) {
			for (int attempt = 0; attempt < 5; ++attempt) {
				glm::vec3 spawnPos;
				if (!findGroundSpawn(player.movement->getPosition(), spawnPos)) continue;
				auto zombie = std::make_shared<Zombie>(spawnPos);
				world->livingEntities.push_back(zombie);
				zombieCount++;
				break;
			}
		}

		// Creepers: lower cap AND a probability gate — only ~25% of attempts are allowed
		// to actually result in a spawn, so creepers are clearly rarer than zombies.
		if (creeperCount < MAX_CREEPERS_PER_PLAYER * static_cast<int>(players.size()) &&
			std::uniform_int_distribution<int>(0, 3)(spawnRng) == 0)
		{
			for (int attempt = 0; attempt < 5; ++attempt) {
				glm::vec3 spawnPos;
				if (!findGroundSpawn(player.movement->getPosition(), spawnPos)) continue;
				auto creeper = std::make_shared<Creeper>(spawnPos);
				world->livingEntities.push_back(creeper);
				creeperCount++;
				break;
			}
		}
	}
}

void Server::despawnDistantMobs()
{
	// Remove any non-player living entity that has no player within DESPAWN_RADIUS (horizontal).
	// Frees up the spawn cap so new mobs appear as players move around the world.
	constexpr float DESPAWN_RADIUS = 200.0f;
	constexpr float DESPAWN_RADIUS_SQ = DESPAWN_RADIUS * DESPAWN_RADIUS;

	for (auto it = world->livingEntities.begin(); it != world->livingEntities.end();) {
		auto &e = *it;
		if (!e || e->getLivingEntityType() == PLAYER) { ++it; continue; }

		float nearestSq = std::numeric_limits<float>::infinity();
		for (auto &p : players) {
			if (!p.movement) continue;
			glm::vec3 d = p.movement->getPosition() - e->getPosition();
			float dsq = d.x * d.x + d.z * d.z;
			if (dsq < nearestSq) nearestSq = dsq;
		}

		if (nearestSq > DESPAWN_RADIUS_SQ) {
			NetEntityMove pkt;
			pkt.eEntityType = e->getEntityType();
			pkt.entityID = e->getID();
			pkt.type = -1;
			pkt.positionX = e->getPositionD().x;
			pkt.positionY = e->getPositionD().y;
			pkt.positionZ = e->getPositionD().z;
			pkt.yaw = e->yaw;
			for (const auto &p : players)
				sendPacketTo(pkt, p.addr);

			it = world->livingEntities.erase(it);
		} else {
			++it;
		}
	}
}

void Server::sendDeaths()
{
	// How many server ticks the body lingers so clients can play the fall-over animation.
	constexpr int32_t DEATH_ANIMATION_TICKS = static_cast<int32_t>(TPS * 1); // ~1s at 20 TPS

	for (auto le = world->livingEntities.begin(); le != world->livingEntities.end();)
	{
		LivingEntity *ent = le->get();

		// Countdown path: already broadcast the death; waiting for the animation window.
		if (ent->pendingDeathRemovalTicks > 0)
		{
			ent->pendingDeathRemovalTicks--;
			if (ent->pendingDeathRemovalTicks == 0)
			{
				if (ent->getLivingEntityType() == PLAYER) {
					ent->onDeath(); // respawn now after animation window
					ent->deathBroadcast = false; // reset for potential respawn
					ent->diedByExplosion = false;
				}
				else {
					le = world->livingEntities.erase(le);
					continue;
				}

			}
			le++;
			continue;
		}

		if (ent->health <= 0 && !ent->deathBroadcast)
		{
			messages.push_back("Someone has died miserably");
			ent->deathBroadcast = true;

			
			NetEntityMove pkt;
			pkt.eEntityType = ent->getEntityType();
			pkt.entityID    = ent->getID();
			pkt.type        = static_cast<uint16_t>(-1);
			pkt.positionX   = ent->getPositionD().x;
			pkt.positionY   = ent->getPositionD().y;
			pkt.positionZ   = ent->getPositionD().z;
			pkt.yaw         = ent->yaw;
			pkt.pitch		= ent->pitch;
			// On the death packet (type==-1), bit 0x20 carries the diedByExplosion truth so the
			// client can pick the right sound (creeper-explode vs creeper-death) without guessing
			// from the priming state (which is true for *any* fused creeper, even when killed
			// before the fuse completes).
			pkt.positionFlags = (ent->diedByExplosion ? 0x20u : 0u);
				
			for (const auto& player : players)
				sendPacketTo(pkt, player.addr);
			
			if (ent->diedByExplosion && ent->getLivingEntityType() != PLAYER)
			{
				// No body left — creepers that self-detonate vanish immediately.
				le = world->livingEntities.erase(le);
				continue;
			}

			// start death animation window
			ent->pendingDeathRemovalTicks = DEATH_ANIMATION_TICKS;
		}
		le++;
	}
}

void Server::sendImGuiData(CPlayerInfo &player) {
    NetImGui pkt;
	const float wx = player.movement->getPosition().x;
	const float wz = player.movement->getPosition().z;
	const TerrainGenerationParams params = world->getTerrainParams();
    const int terrainHeight = ChunkGeneration::computeTerrainHeight(params, wx, wz);

    pkt.currentBiome = static_cast<uint8_t>(ChunkGeneration::computeBiome(params, wx, wz, terrainHeight));
    pkt.terrainHeight = terrainHeight;
    pkt.seaLevel = params.seaLevel;
    pkt.worldSeed = params.seed;
    pkt.continentalness = ChunkGeneration::getContinentalness(params, wx, wz);
    pkt.erosion = ChunkGeneration::getErosion(params, wx, wz);
    pkt.peakValley = ChunkGeneration::getPV(params, wx, wz);
    pkt.temperature = ChunkGeneration::getTemperature(params, wx, wz);
    pkt.humidity = ChunkGeneration::getHumidity(params, wx, wz);

    const auto qc = ChunkGeneration::computeQuantizedClimate(params, wx, wz);
    pkt.contBucket    = qc.continentalness;
    pkt.erosionBucket = qc.erosion;
    pkt.pvBucket      = qc.peakValley;
    pkt.tempBucket    = qc.temperature;
    pkt.humidBucket   = qc.humidity;

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
		CH.flags = CH.flags | PacketFlags::Compressed;
		sendPacketTo(CH, player.addr);

        // 3. Split into packets. Each CHUNK_DATA is Reliable, so the layer
        //    guarantees in-order delivery — no per-fragment sequence needed.
        //    Account for packet encoding overhead: 1(type)+4(seq)+1(flags)+4(X)+4(Z)+4(len) = 18 bytes
        size_t payloadCapacity = MAXLINE - 18;

        for (size_t offset = 0; offset < compressed.size(); offset += payloadCapacity) {
            size_t chunkSize = std::min(payloadCapacity, compressed.size() - offset);

            NetChunkData CD;
			CD.X = chunkPos.first;
			CD.Z = chunkPos.second;
			CD.data.assign(compressed.begin() + offset, compressed.begin() + offset + chunkSize);

			if (compressed.size() - offset <= payloadCapacity)
            	CD.flags = CD.flags | PacketFlags::FinalChunk;

            sendPacketTo(CD, player.addr);
        }
    }
}

// TODO : delta compression
void Server::sendPositionDeltas(CPlayerInfo &player)
{
	NetPlayerMove pkt;

 	pkt.serverClientReconciliationTick = player.movement->getLastAppliedServerClientReconciliationTick();

	pkt.positionX = player.movement->getPositionD().x;
	pkt.positionY = player.movement->getPositionD().y;
	pkt.positionZ = player.movement->getPositionD().z;

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
			if (entity == p.movement || (!entity->positionUpdated && !entity->rotationUpdated && !entity->pendingArmSwing && !entity->pendingHurt)) continue;

			NetEntityMove pkt;

			pkt.eEntityType = entity->getEntityType();
			pkt.entityID = entity->getID();
			pkt.type = static_cast<LivingEntityType>(entity->getLivingEntityType());

			pkt.positionX = entity->getPositionD().x;
			pkt.positionY = entity->getPositionD().y;
			pkt.positionZ = entity->getPositionD().z;

			pkt.yaw = entity->yaw;
			pkt.pitch = entity->pitch;
			pkt.positionFlags = (entity->hasHorizontalInput ? 0x01u : 0u)
			                  | (entity->isOnGround() ? 0x02u : 0u)
			                  | (entity->pendingArmSwing ? 0x04u : 0u)
			                  | (entity->networkedPrimed ? 0x08u : 0u)
			                  | (entity->pendingHurt ? 0x10u : 0u);

			sendPacketTo(pkt, p.addr);
		}

		entity->positionUpdated = false;
		entity->rotationUpdated = false;
		entity->pendingArmSwing = false;
		entity->pendingHurt = false;
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

			pkt.positionX = entity->getPositionD().x;
			pkt.positionY = entity->getPositionD().y;
			pkt.positionZ = entity->getPositionD().z;

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
    // Groups carry state that's always reliable-worthy today (modified blocks,
    // connect handshake batches). Mark the outer envelope reliable so the
    // whole batch is delivered in order. Inner packets are unpacked after
    // ingest and bypass the reliability layer — their own flags are ignored.
    NetPacketGroup group;
    group.flags = group.flags | PacketFlags::Reliable;
    int currSize = 8; // header (6) + u16 group count (2)

	auto it = pkts.begin();
	while (it != pkts.end()) {
		auto buf = encodePacket(**it);
		int nextSize = currSize + 4 + buf.size(); // 4 bytes for per-packet size header

        // if next packet would exceed max size -> send current group first
        if (nextSize > MAXLINE) {
            if (!group.rawPackets.empty()) {
                sendPacketTo(group, cliaddr);
                group = NetPacketGroup();
                group.flags = group.flags | PacketFlags::Reliable;
                currSize = 8;
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

void Server::pingLoop() {
	while (true) {
		// wait until server::loop() sends data
		std::unique_lock<std::mutex> lock(pingMutex);
		pingCV.wait(lock, [this] {return !pingQueue.empty() || !running; });

		if (!running && pingQueue.empty())
			return;

		while (!pingQueue.empty()) {
			// after the wait, we own the lock
			PingJob job = pingQueue.front();
			pingQueue.pop();
			lock.unlock(); // release lock while processing to allow main thread to enqueue more jobs

			NetPong pong;
			pong.timestamp = job.timestamp;
			sendPacketTo(pong, job.addr);  // unicast back to this client only

			lock.lock(); // re-acquire lock before checking queue again
		}
	}
}

void Server::sendPacketTo(Packet& pkt, const sockaddr_in &cliaddr) {
	std::vector<uint8_t> bytes;
	if (hasFlag(pkt.flags, PacketFlags::Reliable)) {
		auto player = NetUtils::findPlayerByAddr(players, cliaddr);
		if (player != players.end()) {
			bytes = reliabilityStamp(player->sendRel, pkt);
		} else {
			// No per-peer state yet (e.g., pre-accept). Send raw; unrecoverable if lost.
			bytes = encodePacket(pkt);
		}
	} else {
		bytes = encodePacket(pkt);
	}

	int n = sendto(sockfd, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0, (sockaddr*)&cliaddr, sizeof(cliaddr));
    if (n < 0) {
#ifdef _WIN32
        std::cerr << "[Network] Failed to send packet type " << static_cast<int>(pkt.type) << " to " << inet_ntoa(cliaddr.sin_addr) << ":" << ntohs(cliaddr.sin_port) << ". Error: " << WSAGetLastError() << std::endl;
#else
        perror("sendto failed");
#endif
    } else {
        if (pkt.type == PacketType::CHUNK_HEADER) {
            // std::cout << "[Network] Sent CHUNK_HEADER to " << inet_ntoa(cliaddr.sin_addr) << ":" << ntohs(cliaddr.sin_port) << " (" << n << " bytes)\n";
        } else if (pkt.type == PacketType::NET_ACCEPT) {
            std::cout << "[Network] Sent NET_ACCEPT to " << inet_ntoa(cliaddr.sin_addr) << ":" << ntohs(cliaddr.sin_port) << " (" << n << " bytes)\n";
        }
    }
}

void Server::sendRawBytesTo(const std::vector<uint8_t>& bytes, const sockaddr_in &cliaddr) {
	sendto(sockfd, reinterpret_cast<const char*>(bytes.data()), static_cast<int>(bytes.size()), 0, (sockaddr*)&cliaddr, sizeof(cliaddr));
}

void Server::reliabilityKeepalive() {
	for (auto& p : players) {
		if (reliabilityShouldKeepalive(p.recvRel, currTick)) {
			NetReliableNack nack;
			nack.fromSeq = p.recvRel.expectedSeq;
			nack.toSeq   = p.recvRel.expectedSeq;
			sendPacketTo(nack, p.addr);
		}
	}
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

			pkt->positionX = entity->getPositionD().x;
			pkt->positionY = entity->getPositionD().y;
			pkt->positionZ = entity->getPositionD().z;

			pkt->yaw = entity->yaw;
			pkt->pitch = entity->pitch;

			groupPkt.push_back(std::move(pkt));
		}

		for (auto &entity : world->itemEntities)
		{
			auto pkt = std::make_unique<NetEntityMove>();

			pkt->eEntityType = entity->getEntityType();
			pkt->entityID = entity->getID();
			pkt->type = static_cast<ItemID>(entity->getItemID());

			pkt->positionX = entity->getPositionD().x;
			pkt->positionY = entity->getPositionD().y;
			pkt->positionZ = entity->getPositionD().z;

			pkt->yaw = entity->yaw;

			groupPkt.push_back(std::move(pkt));
		}

		int twohundred0 = 200;
		int twohundred1 = 200;
		int twohundred2 = 200;
		int twohundred3 = 200;
		player->movement->inventory->insertItemsToSlot(BlockType::DIRT, 0, twohundred0);
		player->movement->inventory->insertItemsToSlot(BlockType::WATER, 8, twohundred1);
		player->movement->inventory->insertItemsToSlot(BlockType::STONE, 1, twohundred2);
		player->movement->inventory->insertItemsToSlot(BlockType::CACTUS, 2, twohundred3);

		auto pkt1 = std::make_unique<NetInventory>();
		pkt1->inventoryTypeID = static_cast<uint8_t>(InventoryType::PLAYER);
		pkt1->amount = 200;
		pkt1->slot = 0;
		pkt1->type = static_cast<std::underlying_type_t<BlockType>>(BlockType::DIRT);

		auto pkt2 = std::make_unique<NetInventory>();
		pkt2->inventoryTypeID = static_cast<uint8_t>(InventoryType::PLAYER);
		pkt2->amount = 200;
		pkt2->slot = 8;
		pkt2->type = static_cast<std::underlying_type_t<BlockType>>(BlockType::WATER);

		auto pkt3 = std::make_unique<NetInventory>();
		pkt3->inventoryTypeID = static_cast<uint8_t>(InventoryType::PLAYER);
		pkt3->amount = 200;
		pkt3->slot = 1;
		pkt3->type = static_cast<std::underlying_type_t<BlockType>>(BlockType::STONE);

		auto pkt4 = std::make_unique<NetInventory>();
		pkt4->inventoryTypeID = static_cast<uint8_t>(InventoryType::PLAYER);
		pkt4->amount = 200;
		pkt4->slot = 2;
		pkt4->type = static_cast<std::underlying_type_t<BlockType>>(BlockType::CACTUS);

		groupPkt.push_back(std::move(pkt1));
		groupPkt.push_back(std::move(pkt2));
		groupPkt.push_back(std::move(pkt3));
		groupPkt.push_back(std::move(pkt4));
	}

	sendNewGroupPacketTo(groupPkt, cliaddr);

	const auto& s = world->getSkyTimeState();
	NetSkyTime skyPkt;
	skyPkt.skyTimeOffset = s.skyTimeOffset;
	skyPkt.sunYawDeg     = s.sunYawDeg;
	skyPkt.skyTimePaused = s.skyTimePaused;
	skyPkt.sunStepping   = s.sunStepping;
	skyPkt.sunPauseTimer = s.sunPauseTimer;
	skyPkt.sunStepTimer  = s.sunStepTimer;
	skyPkt.skyMode       = s.skyMode;
	skyPkt.skyTimeSpeed  = s.skyTimeSpeed;
	sendPacketTo(skyPkt, cliaddr);

	NetAccept acceptPkt;
	if (player != players.end()) {
		acceptPkt.clientId     = static_cast<uint32_t>(player->movement->getID());
		acceptPkt.playerListId = static_cast<uint32_t>(player->id);
	}
	sendPacketTo(acceptPkt, cliaddr);
}

void Server::saveWorldOnExit()
{
	world->saveRegionsOnExit();
}