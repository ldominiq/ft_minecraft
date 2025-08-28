
#ifndef SERVER_HPP
#define SERVER_HPP

#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <iostream>
#include <fcntl.h>
#include <chrono>
#include <thread>
#include <zstd.h>

#include "Protocol.hpp"
#include "World.hpp"
#include "PlayerInfo.hpp"

#define PORT 1234

using TickDuration = std::chrono::steady_clock::duration;
constexpr TickDuration TICK_RATE = std::chrono::duration_cast<TickDuration>(
    std::chrono::duration<double>(1.0 / 60.0)
);

class Server {
private:
    int sockfd;
    uint8_t buffer[MAXLINE];
    struct sockaddr_in servaddr;
	std::chrono::_V2::steady_clock::time_point currTick;

	std::vector<CPlayerInfo> players;

	std::unique_ptr<World> world;

	bool running = false;

	float deltaTime;

	void gameTick();

    void createSocket();
    void fillServerInfo();
    void bindSocket();
    void loop();

	void dispatch(const uint8_t *data, int n, sockaddr_in &clidarr);
	void receiveConnect(NetConnect &pkt, const sockaddr_in &cliaddr);
	void receivePlayerInputs(NetPlayerInputs &pkt, const sockaddr_in &clieaddr);
	void receivePlayerMouseInputs(NetPlayerMouseInputs &pkt, const sockaddr_in &clieddr);

	void sendAll();
	void sendPacketTo(const Packet& pkt, const sockaddr_in &cliaddr);
	void sendAccept(const sockaddr_in &cliaddr);
	void sendImGuiData(CPlayerInfo &player);

	void sendChunk(CPlayerInfo &player);
	void sendPositionDeltas(CPlayerInfo &player);
	void sendNewlyUpdatedBlocks(CPlayerInfo &player, std::vector<std::pair<glm::ivec3, BlockType>> &newlyUpdatedBlocks);

	void saveWorldOnExit();

public:
    Server();
    ~Server();
    void run(); // start server
};

namespace NetUtils {

	// Returns a pointer to the player, or nullptr if not found
	inline CPlayerInfo* findPlayerByAddr(std::vector<CPlayerInfo>& players, const sockaddr_in& addrToFind) {
		auto it = std::find_if(players.begin(), players.end(),
			[&](const CPlayerInfo& p) {
				return p.addr.sin_addr.s_addr == addrToFind.sin_addr.s_addr &&
					p.addr.sin_port == addrToFind.sin_port;
			});
		return it != players.end() ? &(*it) : nullptr;
	}

} // namespace NetUtils

#endif
