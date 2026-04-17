
#include "PlayerListHUD.hpp"
#include <cstdio>
#include <cmath>

glm::vec3 PlayerListHUD::idToColor(uint32_t id)
{
    uint32_t h   = id * 2654435761u;
    float    hue = static_cast<float>(h & 0xFF) / 255.0f * 360.0f;
    float    s   = 0.75f, v = 0.9f;
    float    c   = v * s;
    float    x   = c * (1.0f - std::abs(std::fmod(hue / 60.0f, 2.0f) - 1.0f));
    float    m   = v - c;
    float    r, g, b;
    int sector = static_cast<int>(hue / 60.0f) % 6;
    switch (sector) {
        case 0: r=c; g=x; b=0; break;
        case 1: r=x; g=c; b=0; break;
        case 2: r=0; g=c; b=x; break;
        case 3: r=0; g=x; b=c; break;
        case 4: r=x; g=0; b=c; break;
        default: r=c; g=0; b=x; break;
    }
    return { r+m, g+m, b+m };
}

PlayerListHUD::PlayerListHUD(float width, float height)
    : Menu(width, height)
{
    build();
}

void PlayerListHUD::update(const std::vector<PlayerEntry>& players)
{
    m_players = players;
}

void PlayerListHUD::build()
{
    textRenderer.setProjection(fullscreenWidth, fullscreenHeight);
}

void PlayerListHUD::onRender()
{
    if (m_players.empty()) return;

    const float s        = menuScale;
    const float panelW   = 300.0f * s;
    const float padX     = 10.0f  * s;
    const float padY     = 8.0f   * s;
    const float lineH    = 20.0f  * s;
    const float iconSize = 12.0f  * s;
    const float topGap   = 10.0f  * s;

    const float headerH = lineH + 4.0f * s;
    const float panelH  = padY + headerH + static_cast<float>(m_players.size()) * lineH + padY;

    const float panelX = (static_cast<float>(fullscreenWidth) - panelW) * 0.5f;
    const float panelY = static_cast<float>(fullscreenHeight) - topGap - panelH;

    // Background
    drawSimpleQuad(panelX, panelY, panelW, panelH, glm::vec4(0.0f, 0.0f, 0.0f, 0.65f));

    // Title centered
    const char* title  = "Players";
    float       titleW = static_cast<float>(textRenderer.getPixelSizeOfString(title));
    float       titleX = panelX + (panelW - titleW) * 0.5f;
    float       titleY = panelY + panelH - padY - lineH * 0.5f;
    textRenderer.renderText(title, titleX, titleY, glm::vec3(1.0f));

    // Separator line
    float sepY = panelY + panelH - padY - headerH;
    drawSimpleQuad(panelX + padX, sepY, panelW - padX * 2.0f, std::max(1.0f, s), glm::vec4(1.0f, 1.0f, 1.0f, 0.4f));

    // Rows top-to-bottom
    float  rowY = sepY - lineH;
    char   buf[64];
    char   pingBuf[16];

    for (const auto& e : m_players) {
        // Colored icon
        float     iconY = rowY;
        glm::vec3 col   = idToColor(e.id);
        drawSimpleQuad(panelX + padX, iconY, iconSize, iconSize, glm::vec4(col, 1.0f));

        // Name
        if (e.isLocal)
            snprintf(buf, sizeof(buf), "Player #%u (You)", e.id);
        else
            snprintf(buf, sizeof(buf), "Player #%u", e.id);
        textRenderer.renderText(buf, panelX + padX + iconSize + 6.0f, rowY, glm::vec3(1.0f));

        // Ping right-aligned, color-coded
        if (e.pingMs >= 0.0f)
            snprintf(pingBuf, sizeof(pingBuf), "%.0f ms", e.pingMs);
        else
            snprintf(pingBuf, sizeof(pingBuf), "--");

        float pingW = static_cast<float>(textRenderer.getPixelSizeOfString(pingBuf));
        glm::vec3 pingColor = (e.pingMs < 0.0f)   ? glm::vec3(0.6f, 0.6f, 0.6f) :
                              (e.pingMs < 80.0f)   ? glm::vec3(0.4f, 1.0f, 0.4f) :
                              (e.pingMs < 150.0f)  ? glm::vec3(1.0f, 1.0f, 0.4f) :
                                                     glm::vec3(1.0f, 0.4f, 0.4f);
        textRenderer.renderText(pingBuf, panelX + panelW - padX - pingW, rowY, pingColor);

        rowY -= lineH;
    }
}
