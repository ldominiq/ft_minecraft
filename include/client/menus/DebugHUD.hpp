#ifndef DEBUGHUD_HPP
#define DEBUGHUD_HPP

#include "Menu.hpp"
#include <cstddef>
#include <cstdio>


struct DebugStats {
    float  fps              = 0.0f;
	float  cpuFrameMs       = 0.0f;
    size_t triangles        = 0;
    size_t cubes            = 0;
    size_t visibleChunks    = 0;
    size_t totalChunks      = 0;
    size_t terrainDrawCalls = 0;
	float  pingMs           = -1.0f;
    glm::vec3 playerPos{};
};

class DebugHUD : public Menu {
public:
    DebugHUD(float width, float height);

    void update(const DebugStats& stats);
    void resize(float width, float height) override;

protected:
    void build() override;
    void onRender() override;

private:
    DebugStats m_stats{};
};

#endif // DEBUGHUD_HPP