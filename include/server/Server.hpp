
#ifndef SERVER_HPP
#define SERVER_HPP

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#define close closesocket
#else
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <thread>
#include <chrono>
#include <zstd.h>

#include <mutex>
#include <condition_variable>
#include <queue>

#include "Protocol.hpp"
#include "World.hpp"
#include "PlayerInfo.hpp"
#include "ItemEntity.hpp"
#include "Config.hpp"

class Server {
private:
    int sockfd;
    uint8_t buffer[MAXLINE];
    struct sockaddr_in servaddr;
	std::chrono::steady_clock::time_point currTick;

	std::vector<CPlayerInfo> players;
	std::deque<std::string> messages;
	std::unique_ptr<World> world;

	std::atomic<bool> running = false;

	int32_t tick = 0;
	float deltaTime;
	
	void updateSkyTime(NetSkyTime &pkt);
	void broadcastSkyTime();

	std::vector<std::thread> dumpThreads;
	std::mutex dumpThreadsMutex;

	struct PingJob {
		sockaddr_in addr;
		uint64_t timestamp;
	};
	std::queue<PingJob> pingQueue;
	std::mutex pingMutex;
	std::condition_variable pingCV;
	std::thread pingThread;

	void pingLoop();

	void gameTick();

    void createSocket();
    void fillServerInfo();
    void bindSocket();
    void loop();

	void dispatch(const uint8_t *data, int n, sockaddr_in &clidarr);
	void dispatchPacket(PacketPtr &pkt, sockaddr_in &cliaddr);
	void receiveConnect(NetConnect &pkt, const sockaddr_in &cliaddr);
	void receiveDisconnect(NetDisconnect &pkt, const sockaddr_in &cliaddr);
	void receivePlayerInputs(NetPlayerInputs &pkt, const sockaddr_in &clieaddr);
	void receivePlayerMouseInputs(NetPlayerMouseInputs &pkt, const sockaddr_in &clieddr);
	void receiveMessage(NetMessage &pkt, const sockaddr_in &cliaddr);
	void receiveTerrainParams(NetTerrainParams &pkt, const sockaddr_in &cliaddr);
	void receiveInventoryAction(NetInventoryAction &pkt, const sockaddr_in &cliaddr);
	// void sendInventorySlot(int slot, NetInventoryAction &pkt, const sockaddr_in &cliaddr);

	void sendAll();
	void sendNewGroupPacketTo(std::vector<PacketPtr> &pkts, const sockaddr_in &cliaddr);
	void sendPacketTo(const Packet& pkt, const sockaddr_in &cliaddr);
	void sendAccept(const sockaddr_in &cliaddr);
	
	void sendDeaths();
	void sendMessage(CPlayerInfo &player);
	void sendImGuiData(CPlayerInfo &player);
	void sendChunk(CPlayerInfo &player);
	void sendPositionDeltas(CPlayerInfo &player);
	void sendNewlyUpdatedBlocks(CPlayerInfo &player);
	void sendEntitiesPositionDeltas();

public:
    Server();
    ~Server();
    void run(std::optional<int> &seed); // start server

	void saveWorldOnExit();
};

namespace NetUtils {

    // Returns an iterator to the player, or players.end() if not found
    inline std::vector<CPlayerInfo>::iterator findPlayerByAddr(
        std::vector<CPlayerInfo>& players,
        const sockaddr_in& addrToFind )
	{
        return std::find_if(players.begin(), players.end(),
            [&](CPlayerInfo& p) {
                return p.addr.sin_addr.s_addr == addrToFind.sin_addr.s_addr &&
                       p.addr.sin_port == addrToFind.sin_port;
            });
    }

} // namespace NetUtils

#endif
