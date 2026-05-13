
#ifndef PLAYERLISTHUD_HPP
#define PLAYERLISTHUD_HPP

#include "Menu.hpp"
#include <vector>
#include <cstdint>

struct PlayerEntry {
    uint32_t id;
	std::string name;
    bool     isLocal;
    float    pingMs;   // -1 = unknown, >=0 = known
};

class PlayerListHUD : public Menu {
public:
    PlayerListHUD(float width, float height);

    void update(const std::vector<PlayerEntry>& players);

protected:
    void build() override;
    void onRender() override;

private:
    std::vector<PlayerEntry> m_players;

    static glm::vec3 idToColor(uint32_t id);
};

#endif // PLAYERLISTHUD_HPP
