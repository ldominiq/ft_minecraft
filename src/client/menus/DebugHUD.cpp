
#include "DebugHUD.hpp"

DebugHUD::DebugHUD(float width, float height)
    : Menu(width, height)
{
    build();
}

void DebugHUD::update(const DebugStats& stats)
{
    m_stats = stats;
}

void DebugHUD::build()
{
    textRenderer.setProjection(fullscreenWidth, fullscreenHeight);
}

void DebugHUD::resize(float width, float height)
{
    // Keep design == screen so menuScale stays 1.0 and we use raw pixel coords.
    DESIGN_WIDTH  = static_cast<int>(width);
    DESIGN_HEIGHT = static_cast<int>(height);
    Menu::resize(width, height);
}

void DebugHUD::onRender()
{
    constexpr float pad = 8.0f;
    constexpr float lineH = 18.0f;
    constexpr float bgPadX = 2.0f;   // horizontal inset inside each bg
    const glm::vec4 bgColor{ 0.0f, 0.0f, 0.0f, 0.0f };
    const glm::vec3 white{ 1.0f, 1.0f, 1.0f };

    const float textX = pad;
    float textY = static_cast<float>(fullscreenHeight) - pad - 4.0f;

    char buf[64];

    auto drawLine = [&](const char* text) {
        float w = static_cast<float>(textRenderer.getPixelSizeOfString(text));
        drawSimpleQuad(textX - bgPadX, textY - 3.0f, w + bgPadX * 2.0f, lineH - 2.0f, bgColor);
        textRenderer.renderText(text, textX, textY, white);
        textY -= lineH;
    };

    snprintf(buf, sizeof(buf), "FPS: %.0f", m_stats.fps);
    drawLine(buf);

    snprintf(buf, sizeof(buf), "Render time: %.2f ms", m_stats.cpuFrameMs);
    drawLine(buf);

    snprintf(buf, sizeof(buf), "Player Position: X=%.0f Y=%.0f Z=%.0f", m_stats.playerPos.x, m_stats.playerPos.y, m_stats.playerPos.z);
    drawLine(buf);

    snprintf(buf, sizeof(buf), "Triangles: %zu", m_stats.triangles);
    drawLine(buf);

    snprintf(buf, sizeof(buf), "Cubes: ~%zu", m_stats.cubes);
    drawLine(buf);

    snprintf(buf, sizeof(buf), "Chunks: %zu / %zu", m_stats.visibleChunks, m_stats.totalChunks);
    drawLine(buf);

    snprintf(buf, sizeof(buf), "Terrain Draw Calls: %zu", m_stats.terrainDrawCalls);
    drawLine(buf);

    snprintf(buf, sizeof(buf), "Ping: %.0f ms", m_stats.pingMs);
    drawLine(buf);

}
